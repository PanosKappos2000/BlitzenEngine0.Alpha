#include "vulkanRenderData.h"

namespace BlitzenRendering
{
    void Node::UpdateTransform(const glm::mat4& parentTransform)
    {
        worldTransform = parentTransform * localTransform;
        for(Node& child : m_children)
        {
            child.UpdateTransform(worldTransform);
        }
    }

    void Node::AddToDrawContext(const glm::mat4& topMatrix, DrawContext& drawContext)
    {
        switch(type)
        {
            case NodeType::NT_Undefined:
            {
                for(Node& child : m_children)
                {
                    child.AddToDrawContext(topMatrix, drawContext);
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
                    newObject.vertexBufferOffset = surface.vertexBufferOffset;
                }
                break;

            }
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
            newObject.vertexBufferOffset = surface.vertexBufferOffset;
        }

        Node::AddToDrawContext(topMatrix, drawContext);
    }
}