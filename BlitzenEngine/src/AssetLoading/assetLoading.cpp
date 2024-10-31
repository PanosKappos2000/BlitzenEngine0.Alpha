#include "assetLoading.h"

namespace BlitzenEngine
{
    void LoadMeshAsset(std::filesystem::path filepath, BlitzenRendering::VulkanRenderer* pVulkan)
    {
        std::cout << "Loading GLTF: " << filepath << '\n';

        //Load the file on to a data buffer
        fastgltf::GltfDataBuffer data;
        data.loadFromFile(filepath);
        
        //Specify flags for the parser's loading function
        fastgltf::Options gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

        //Declare the parser and an object to hold the asset that it will load
        fastgltf::Asset gltf;
        fastgltf::Parser parser{};

        fastgltf::Expected load = parser.loadBinaryGLTF(&data, filepath.parent_path(), gltfOptions);
        if (load) 
        {
            //If the parser succeeds move the loaded memory to the asset object
            gltf = std::move(load.get());
        }
        else 
        {
            std::cout << "Loading GLTF : " << filepath << " -> Process failed\n";
        }

        //Placehoder arrays to load the vertex and index buffer and pass them to the mesh buffers
        std::vector<BlitzenRendering::VulkanVertex> vertices;
        std::vector<uint32_t> indices;

        //Start iterating through all the meshes that were load from gltf
        for(size_t i = 0; i < gltf.meshes.size(); ++i)
        {
            //Clear the vertices and indices so that the next mesh can be processed
            vertices.clear();
            indices.clear();
            //Add a new mesh to the vulkan mesh assets array and save its name
            pVulkan->m_assets.push_back(BlitzenRendering::VulkanMeshAsset());
            pVulkan->m_assets[i].meshName = gltf.meshes[i].name;
            for (auto& primitive : gltf.meshes[i].primitives)
            {
                //Create a new surface to represent the current primitive in the mesh list
                BlitzenRendering::GeoSurface newSurface;
                newSurface.firstIndex = indices.size();
                newSurface.indexCount = gltf.accessors[primitive.indicesAccessor.value()].count;
                pVulkan->m_assets[i].geoSurfaces.push_back(newSurface);

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
            }
            constexpr bool OverrideColors = true;
            if (OverrideColors) 
            {
                for (BlitzenRendering::VulkanVertex& vtx : vertices) 
                {
                    vtx.color = glm::vec4(vtx.normal, 1.f);
                }
            }

            //Load all the vertices and indices that were collected to the mesh buffers
            pVulkan->LoadMeshBuffers(pVulkan->m_assets[pVulkan->m_assets.size() - 1].meshBuffers,
                vertices, indices);
        }
    }
}