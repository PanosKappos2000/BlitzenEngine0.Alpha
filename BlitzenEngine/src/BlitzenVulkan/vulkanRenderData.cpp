#include "vulkanRenderData.h"
#include "VulkanRenderer.h"

namespace BlitzenRendering
{
    void Node::UpdateTransform(const glm::mat4& parentTransform)
    {
        worldTransform = parentTransform * localTransform;
        for(Node* child : m_children)
        {
            child->UpdateTransform(worldTransform);
        }
    }

    void Node::AddToDrawContext(const glm::mat4& topMatrix, DrawContext& drawContext)
    {
        //This code worked but it was wrong, I am keeping it to see what the proper way to do it fixes
        /*switch(type)
        {
            case NodeType::NT_Undefined:
            {
                for(Node* child : m_children)
                {
                    child->AddToDrawContext(topMatrix, drawContext);
                }
                break;
            }
            case NodeType::NT_MeshNode:
            {
                glm::mat4 nodeMatrix = topMatrix * worldTransform;

                for(GeoSurface& surface : m_asset->geoSurfaces )
                {
                    drawContext.opaqueObjects.push_back(VulkanRenderObject());
                    VulkanRenderObject& newObject = drawContext.opaqueObjects.back();
                    newObject.firstIndex = surface.firstIndex;
                    newObject.indexCount = surface.indexCount;
                    newObject.pMaterial = surface.pMaterial;
                    newObject.transform = nodeMatrix;
                }
                break;

            }
        }*/

        if(type == NodeType::NT_MeshNode)
        {
             glm::mat4 nodeMatrix = topMatrix * worldTransform;
    
             for(GeoSurface& surface : m_asset->geoSurfaces )
             {
                 drawContext.opaqueObjects.push_back(VulkanRenderObject());
                 VulkanRenderObject& newObject = drawContext.opaqueObjects.back();
                 newObject.firstIndex = surface.firstIndex;
                 newObject.indexCount = surface.indexCount;
                 newObject.pMaterial = surface.pMaterial;
                 newObject.transform = nodeMatrix;
             }
        }
    
        for(Node* child : m_children)
        {
            child->AddToDrawContext(topMatrix, drawContext);
        }
    }

    void LoadedGLTFScene::AddToDrawContext(const glm::mat4& topMatrix, DrawContext& drawContext)
    {
        for(Node* node : m_pureParentNodes)
        {
            node->AddToDrawContext(topMatrix, drawContext);
        }
    }
    void LoadedGLTFScene::ClearAll()
    {
        materialDataBuffer.CleanupResources(m_pRenderer->m_device, m_pRenderer->m_allocator);

        for (auto& [key, value] : m_textures) {
        
        if (value.image == m_pRenderer->m_placeholderErrorTextureImage.image) {
            //dont destroy the default images
            continue;
        }
        value.CleanupResources(m_pRenderer->m_device, m_pRenderer->m_allocator);
    }
    }

    void MeshNode::AddToDrawContext(const glm::mat4& topMatrix, DrawContext& drawContext)
    {
        glm::mat4 nodeMatrix = topMatrix * worldTransform;

        for(GeoSurface& surface : m_asset->geoSurfaces )
        {
            drawContext.opaqueObjects.push_back(VulkanRenderObject());
            VulkanRenderObject& newObject = drawContext.opaqueObjects.back();
            newObject.firstIndex = surface.firstIndex;
            newObject.indexCount = surface.indexCount;
            newObject.pMaterial = surface.pMaterial;
            newObject.transform = nodeMatrix;
        }

        Node::AddToDrawContext(topMatrix, drawContext);
    }
}