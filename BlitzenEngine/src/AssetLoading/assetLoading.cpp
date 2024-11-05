#include "assetLoading.h"

namespace BlitzenEngine
{
    void LoadScene(std::string_view filepath, BlitzenRendering::VulkanRenderer* pVulkan, BlitzenRendering::LoadedGLTF& scene)
    {
        std::cout << "Loading GLTF: " << filepath << '\n';

        scene.pVulkan = pVulkan;

        fastgltf::Parser parser {};

        constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble 
        | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
        // fastgltf::Options::LoadExternalImages;

        fastgltf::GltfDataBuffer data;
        data.loadFromFile(filepath);

        fastgltf::Asset gltf;

        std::filesystem::path path = filepath;

        auto type = fastgltf::determineGltfFileType(&data);
        if (type == fastgltf::GltfType::glTF) 
        {
            auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
            if (load) 
            {
                gltf = std::move(load.get());
            } else 
            {
                std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
                return;
            }
        } 
        else if (type == fastgltf::GltfType::GLB) 
        {
            auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
            if (load) 
            {
                gltf = std::move(load.get());
            } 
            else 
            {
                std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
                return;
            }
        } 
        else 
        {
            std::cerr << "Failed to determine glTF container" << std::endl;
            return;
        }

        //Create its own descriptor allocator, the scene has access to the vulkan device since it is declared as a friend class
        scene.descriptorAllocator.Init(pVulkan->m_device);

        /*
        Extracting the samplers for all textures in the gltf scene
        */
        for(fastgltf::Sampler& gltfSampler : gltf.samplers)
        {
            VkSamplerCreateInfo vulkanSamplerInfo{};
            vulkanSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            vulkanSamplerInfo.maxLod = VK_LOD_CLAMP_NONE;
            vulkanSamplerInfo.minLod = 0;

            //Get the mag filter from the gltf, if it doesn't have anything it defaults to nearest
            fastgltf::Filter gltfMagFilter = gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest);
            if(gltfMagFilter == fastgltf::Filter::Nearest || gltfMagFilter == fastgltf::Filter::NearestMipMapLinear 
            || gltfMagFilter == fastgltf::Filter::NearestMipMapNearest)
            {
                vulkanSamplerInfo.magFilter = VK_FILTER_NEAREST;
            }
            else
            {
                vulkanSamplerInfo.magFilter = VK_FILTER_LINEAR;
            }

            //Same thing happens with the min filter
            fastgltf::Filter gltfMinFilter = gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest);
            if(gltfMinFilter == fastgltf::Filter::Nearest || gltfMinFilter == fastgltf::Filter::NearestMipMapLinear 
            || gltfMinFilter == fastgltf::Filter::NearestMipMapNearest)
            {
                vulkanSamplerInfo.minFilter = VK_FILTER_NEAREST;
            }
            else
            {
                vulkanSamplerInfo.minFilter = VK_FILTER_LINEAR;
            }

            //Retrives the mip map mode in a similar fashion with the default being linear
            if(gltfMinFilter == fastgltf::Filter::NearestMipMapNearest || gltfMinFilter == fastgltf::Filter::LinearMipMapNearest)
            {
                vulkanSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            }
            else
            {
                vulkanSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            }

            //Puts a new vulkan sampler object at the back of the array in the gltf scene and initializes it with vkCreateSampler
            scene.textureSamplers.push_back(VkSampler());
            vkCreateSampler(pVulkan->m_device, &vulkanSamplerInfo, nullptr, &(scene.textureSamplers.back()));
        }

        //Becuase fastgltf uses indices, each part of the scene will be temporarily reference by an array
        std::vector<BlitzenRendering::VulkanMeshAsset*> meshAssets;
        std::vector<BlitzenRendering::Node*> nodes;
        std::vector<BlitzenRendering::VulkanAllocatedImage*> textureImages;
        std::vector<BlitzenRendering::MaterialInstance*> materials;

        //Loading textures, only the renderer's default for now
        for(fastgltf::Image image : gltf.images)
        {
            textureImages.push_back(&(pVulkan->m_placeholderErrorTextureImage));
        }

        //Allocate a buffer for material constants and resource and retrieve a pointer to its allocation
        pVulkan->AllocateBuffer(scene.materialDataBuffer, sizeof(BlitzenRendering::MaterialConstants) * gltf.materials.size(), 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        int materialDataIndex = 0;
        BlitzenRendering::MaterialConstants* materialData = reinterpret_cast<BlitzenRendering::MaterialConstants*>(
        scene.materialDataBuffer.allocationInfo.pMappedData);

        /*Load materials*/
        for(fastgltf::Material& gltfMaterial : gltf.materials)
        {
            //Add a new entry in the materials of the scene and add a reference to it in the placeholder array
            scene.materials[gltfMaterial.name.c_str()] = BlitzenRendering::MaterialInstance();
            materials.push_back(&(scene.materials[gltfMaterial.name.c_str()]));

            BlitzenRendering::MaterialConstants materialConstants;
            //Get the base color data of the material
            materialConstants.colorFactors.x = gltfMaterial.pbrData.baseColorFactor[0];
            materialConstants.colorFactors.y = gltfMaterial.pbrData.baseColorFactor[1];
            materialConstants.colorFactors.z = gltfMaterial.pbrData.baseColorFactor[2];
            materialConstants.colorFactors.w = gltfMaterial.pbrData.baseColorFactor[3];
            //Get the metallic and rough factors of the material
            materialConstants.metalRoughFactors.x = gltfMaterial.pbrData.metallicFactor;
            materialConstants.metalRoughFactors.y = gltfMaterial.pbrData.roughnessFactor;
            //Pass all the data that was retrieved to the material buffer
            materialData[materialDataIndex] = materialConstants;

            //Simple distinction between transparent and opaque objects, not doing anything with transparent materials for now
            BlitzenRendering::MaterialPass passType = BlitzenRendering::MaterialPass::MP_opaqueMaterial;
            /*if (gltfMaterial.alphaMode == fastgltf::AlphaMode::Blend)
            {
                passType = BlitzenRendering::MaterialPass::MP_transparentMaterial;
            }*/

            //Load placeholder resources for the material
            BlitzenRendering::MaterialResources materialResources;
            materialResources.colorImage = pVulkan->m_placeholderGreyTextureImage;
            materialResources.colorSampler = pVulkan->m_placeholderLinearSampler;
            materialResources.metalRoughImage = pVulkan->m_placeholderGreyTextureImage;
            materialResources.metalRoughSampler = pVulkan->m_placeholderLinearSampler;

            materialResources.dataBuffer = scene.materialDataBuffer.buffer;
            materialResources.dataBufferOffset = materialDataIndex * sizeof(BlitzenRendering::MaterialConstants);

            if(gltfMaterial.pbrData.baseColorTexture.has_value())
            {
                //Get the index of the texture used by this material
                size_t materialColorImageIndex = gltf.textures[gltfMaterial.pbrData.baseColorTexture.value().textureIndex].
                imageIndex.value();
                size_t materialColorSamplerIndex = gltf.textures[gltfMaterial.pbrData.baseColorTexture.value().textureIndex].
                samplerIndex.value();

                materialResources.colorImage = *textureImages[materialColorImageIndex];
                materialResources.colorSampler = scene.textureSamplers[materialColorSamplerIndex];
            }

            //Once all data has been retrieved, it can be passed to the descriptors
            scene.descriptorAllocator.AllocateDescriptorSet(pVulkan->m_device, materials.back()->descriptorSet, 
            pVulkan->m_placeholderMaterialData.materialLayout);
            pVulkan->WriteMaterial(scene.materials[gltfMaterial.name.c_str()], pVulkan->m_device, passType, materialResources);
            materialDataIndex++;
        }



        std::vector<BlitzenRendering::VulkanVertex> vertices;
        std::vector<uint32_t> indices;
        /* Load each mesh in the gltf scene*/
        for(size_t i = 0; i < gltf.meshes.size(); ++i)
        {
            //Clear the vertices and indices so that the next mesh can be processed
            vertices.clear();
            indices.clear();
            //Add a new mesh to the vulkan mesh assets array and save its name
            scene.meshNodes[gltf.meshes[i].name.c_str()] = BlitzenRendering::VulkanMeshAsset();
            meshAssets.push_back(&(scene.meshNodes[gltf.meshes[i].name.c_str()]));
            meshAssets.back()->meshName = gltf.meshes[i].name;
            for (auto& primitive : gltf.meshes[i].primitives)
            {
                //Create a new surface to represent the current primitive in the mesh list
                BlitzenRendering::GeoSurface newSurface;
                newSurface.firstIndex = indices.size();
                newSurface.indexCount = gltf.accessors[primitive.indicesAccessor.value()].count;

                size_t initialVertex = vertices.size();

                /* Load indices */
                fastgltf::Accessor& indexaccessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) 
                    {
                        indices.push_back(idx + initialVertex);
                    });

                /* Load vertex positions */
                fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        BlitzenRendering::VulkanVertex newVertex;
                        newVertex.position = v;
                        newVertex.normal = { 1, 0, 0 };
                        newVertex.color = glm::vec4{ 1.f };
                        newVertex.uv_x = 0;
                        newVertex.uv_y = 0;
                        vertices[initialVertex + index] = newVertex;
                    });

                /* Load normals */
                auto normals = primitive.findAttribute("NORMAL");
                if (normals != primitive.attributes.end()) 
                {
                
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                        [&](glm::vec3 v, size_t index) 
                        {
                            vertices[initialVertex + index].normal = v;
                        });
                }

                /* Load uv maps */
                auto uv = primitive.findAttribute("TEXCOORD_0");
                if (uv != primitive.attributes.end()) {
                
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                        [&](glm::vec2 v, size_t index) 
                        {
                            vertices[initialVertex + index].uv_x = v.x;
                            vertices[initialVertex + index].uv_y = v.y;
                        });
                }

                /* Load vertex colors */
                auto colors = primitive.findAttribute("COLOR_0");
                if (colors != primitive.attributes.end())
                {
                
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                        [&](glm::vec4 v, size_t index) 
                        {
                            vertices[initialVertex + index].color = v;
                        });
                }

                //If the primitive has a material, it retrieves its index and saves it, otherwise it get the first material
                if (primitive.materialIndex.has_value())
                {
                    newSurface.pMaterial = materials[primitive.materialIndex.value()];
                } 
                else 
                {
                    newSurface.pMaterial = materials[0];
                }
                //With the vertex surface completed it can now be added to the surfaces of the current mesh
                meshAssets.back()->geoSurfaces.push_back(newSurface);
            }

            //Load all the vertices and indices that were collected to the mesh buffers
            pVulkan->LoadMeshBuffers(meshAssets.back()->meshBuffers, vertices, indices);
        }

        /* Load each node in the gltf scene */
        for (fastgltf::Node& node : gltf.nodes)
        {
            //Add a new entry to the scene's hash map and add a pointer to it in the nodes array
            scene.nodes[node.name.c_str()] = BlitzenRendering::Node();
            nodes.push_back(&(scene.nodes[node.name.c_str()]));
            //Find the nodes with a mesh and save their mesh asset and swith their type to mesh node
            if(node.meshIndex.has_value())
            {
                nodes.back()->m_asset = meshAssets[*node.meshIndex];
                nodes.back()->type = BlitzenRendering::NodeType::NT_MeshNode;
            }
            //Specify the nodes that do not have a mesh
            else
            {
                nodes.back()->type = BlitzenRendering::NodeType::NT_Undefined;
            }

            std::visit(fastgltf::visitor { [&](fastgltf::Node::TransformMatrix matrix) {
                                          memcpy(&(nodes.back()->localTransform), matrix.data(), sizeof(matrix));
                                          },
                                          [&](fastgltf::Node::TRS transform) {
                                            glm::vec3 tl(transform.translation[0], transform.translation[1],
                                                transform.translation[2]);
                                            glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                                                transform.rotation[2]);
                                            glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                                            glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                                            glm::mat4 rm = glm::toMat4(rot);
                                            glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                                            nodes.back()->localTransform = tm * rm * sm;
                                            } }, node.transform);
            
        }
        for (int i = 0; i < gltf.nodes.size(); ++i)
        {
            BlitzenRendering::Node* pParent = nodes[i];
            for (auto& child : gltf.nodes[i].children)
            {
                //Add each child to the parent node
                pParent->m_children.push_back(nodes[child]);
                //Give every child a pointer to the parent node
                pParent->m_children.back()->pParentNode = pParent;
            }
        }
        for(auto& node : nodes)
        {
            if(!(node->pParentNode))
            {
                //Add every node that does not have a parent to the top nodes and refresh its children's transform
                scene.topNodes.push_back(node);
                node->UpdateTransform(glm::mat4(1.f));
            }
        }
    }

}