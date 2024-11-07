#include "assetLoading.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace BlitzenEngine
{
    void LoadScene(std::string_view filepath, std::vector<BlitzenRendering::VulkanVertex>& vertices,
	std::vector<uint32_t>&	indices, BlitzenRendering::LoadedGLTFScene& scene, BlitzenRendering::VulkanRenderer* pVulkan)
    {
        std::cout << "Loading GLTF: " << filepath << '\n';

        scene.m_pRenderer = pVulkan;

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



        //Creates its own descriptor allocator
        scene.m_descriptorAllocator.Init(pVulkan->m_device);


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
            scene.m_textureSamplers.push_back(VkSampler());
            vkCreateSampler(pVulkan->m_device, &vulkanSamplerInfo, nullptr, &(scene.m_textureSamplers.back()));
        }

        //Since fastgltf uses indices, each part of the scene will be temporarily referenced by an array
        std::vector<BlitzenRendering::VulkanMeshAsset*> meshAssets;
        std::vector<BlitzenRendering::Node*> nodes;
        std::vector<BlitzenRendering::VulkanAllocatedImage*> textureImages;
        std::vector<BlitzenRendering::MaterialInstance*> materials;

        //Loading textures, only the renderer's default for now
        for(fastgltf::Image image : gltf.images)
        {
            scene.m_textures[image.name.c_str()] = BlitzenRendering::VulkanAllocatedImage();
            LoadImage(scene.m_textures[image.name.c_str()], pVulkan, gltf, image);
            if(scene.m_textures[image.name.c_str()].image != VK_NULL_HANDLE)
            {
                textureImages.push_back(&(scene.m_textures[image.name.c_str()]));
            }
            else
            {
                scene.m_textures.erase(image.name.c_str());
                textureImages.push_back(&(pVulkan->m_placeholderErrorTextureImage));
            }
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
            scene.m_materials[gltfMaterial.name.c_str()] = BlitzenRendering::MaterialInstance();
            materials.push_back(&(scene.m_materials[gltfMaterial.name.c_str()]));

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
                materialResources.colorSampler = scene.m_textureSamplers[materialColorSamplerIndex];
            }

            //Once all data has been retrieved, it can be passed to the descriptors
            scene.m_descriptorAllocator.AllocateDescriptorSet(pVulkan->m_device, materials.back()->descriptorSet, 
            pVulkan->m_placeholderMaterialData.materialLayout);
            pVulkan->WriteMaterial(scene.m_materials[gltfMaterial.name.c_str()], pVulkan->m_device, passType, materialResources);
            materialDataIndex++;
        }

        //Start iterating through all the meshes that were loaded from gltf
        for(size_t i = 0; i < gltf.meshes.size(); ++i)
        {
            //Add a new mesh to the vulkan mesh assets array and save its name
            scene.m_meshAssets[gltf.meshes[i].name.c_str()] = BlitzenRendering::VulkanMeshAsset();
            meshAssets.push_back(&(scene.m_meshAssets[gltf.meshes[i].name.c_str()]));
            meshAssets.back()->meshName = gltf.meshes[i].name;
            //Iterate through all the surfaces in the mesh asset
            for (auto& primitive : gltf.meshes[i].primitives)
            {
                //Add a new geoSurface object at the back of the mesh assets array and update its indices
                meshAssets.back()->geoSurfaces.push_back(BlitzenRendering::GeoSurface());
                meshAssets.back()->geoSurfaces.back().firstIndex = indices.size();
                meshAssets.back()->geoSurfaces.back().indexCount = gltf.accessors[primitive.indicesAccessor.value()].count;

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
                    meshAssets.back()->geoSurfaces.back().pMaterial = materials[primitive.materialIndex.value()];
                } 
                else 
                {
                    meshAssets.back()->geoSurfaces.back().pMaterial = materials[0];
                }
            }

        }

        /* Load each node in the gltf scene */
        for (fastgltf::Node& node : gltf.nodes)
        {
            //Add a new entry to the scene's hash map and add a pointer to it in the nodes array
            scene.m_nodes[node.name.c_str()] = BlitzenRendering::Node();
            nodes.push_back(&(scene.m_nodes[node.name.c_str()]));
            //Find the nodes with a mesh, save their mesh asset and swith their type to mesh node
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

            //Update the local transofrm of each node
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

        //Having loaded all the nodes, iterate through them to update their parent-children relationships
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

        //Iterates through them once more to find the no-parent nodes and update the transform of their children
        for(auto& node : nodes)
        {
            if(!(node->pParentNode))
            {
                //Add every node that does not have a parent to the top nodes and refresh its children's transform
                scene.m_pureParentNodes.push_back(node);
                node->UpdateTransform(glm::mat4(1.f));
            }
        }
    }


    void LoadImage(BlitzenRendering::VulkanAllocatedImage& imageToLoad, BlitzenRendering::VulkanRenderer* pVulkan, 
    fastgltf::Asset& gltfAsset, fastgltf::Image& gltfImage)
    {
        int width; 
        int height;
        int nrChannels;
    
        std::visit(
            fastgltf::visitor {
                [](auto& arg) {},
                [&](fastgltf::sources::URI& filePath) {
                    assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                    assert(filePath.uri.isLocalPath()); // We're only capable of loading
                                                        // local files.
    
                    const std::string path(filePath.uri.path().begin(),
                        filePath.uri.path().end()); // Thanks C++.
                    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                    if (data) {
                        VkExtent3D imagesize;
                        imagesize.width = width;
                        imagesize.height = height;
                        imagesize.depth = 1;
    
                        pVulkan->AllocateImage(data, imageToLoad, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
    
                        stbi_image_free(data);
                    }
                },
                [&](fastgltf::sources::Vector& vector) {
                    unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                        &width, &height, &nrChannels, 4);
                    if (data) {
                        VkExtent3D imagesize;
                        imagesize.width = width;
                        imagesize.height = height;
                        imagesize.depth = 1;
    
                        pVulkan->AllocateImage(data, imageToLoad, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);
    
                        stbi_image_free(data);
                    }
                },
                [&](fastgltf::sources::BufferView& view) {
                    auto& bufferView = gltfAsset.bufferViews[view.bufferViewIndex];
                    auto& buffer = gltfAsset.buffers[bufferView.bufferIndex];
    
                    std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                                                   // specify LoadExternalBuffers, meaning all buffers
                                                   // are already loaded into a vector.
                                   [](auto& arg) {},
                                   [&](fastgltf::sources::Vector& vector) {
                                       unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                           static_cast<int>(bufferView.byteLength),
                                           &width, &height, &nrChannels, 4);
                                       if (data) {
                                           VkExtent3D imagesize;
                                           imagesize.width = width;
                                           imagesize.height = height;
                                           imagesize.depth = 1;
    
                                           pVulkan->AllocateImage(data, imageToLoad, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                               VK_IMAGE_USAGE_SAMPLED_BIT,false);
    
                                           stbi_image_free(data);
                                       }
                                   } },
                        buffer.data);
                },
            },
            gltfImage.data);
    }
}