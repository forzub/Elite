#include "src/render/celestial/CelestialShapeMesh.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "render/tiny_obj_loader.h"
#include "src/render/bitmap/TextureLoader.h"

namespace render::celestial
{
    const CelestialShapeMesh* CelestialShapeMeshLibrary::load(
        const std::string& objPath,
        const std::string& albedoPath,
        bool flipTexcoordV
    )
    {
        if (objPath.empty())
            return nullptr;

        const std::filesystem::path resolvedObj =
            std::filesystem::path(objPath).lexically_normal();

        const std::string key =
            resolvedObj.generic_string();

        auto existing =
            m_meshByPath.find(
                key
            );

        if (existing != m_meshByPath.end())
            return &existing->second;

        auto attempted =
            m_meshLoadAttemptedByPath.find(
                key
            );

        if (attempted != m_meshLoadAttemptedByPath.end() &&
            attempted->second)
        {
            return nullptr;
        }

        m_meshLoadAttemptedByPath[key] =
            true;

        CelestialShapeMesh mesh;

        if (!loadObjMesh(
            key,
            mesh,
            flipTexcoordV
        ))
        {
            return nullptr;
        }

        mesh.albedoTexture =
            loadAlbedoTexture(
                albedoPath
            );

        auto inserted =
            m_meshByPath.emplace(
                key,
                std::move(mesh)
            );

        return &inserted.first->second;
    }

    bool CelestialShapeMeshLibrary::loadObjMesh(
        const std::string& objPath,
        CelestialShapeMesh& outMesh,
        bool flipTexcoordV
    )
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        std::string warn;
        std::string err;

        const bool ok =
            tinyobj::LoadObj(
                &attrib,
                &shapes,
                &materials,
                &warn,
                &err,
                objPath.c_str(),
                nullptr,
                true
            );

        if (!ok)
        {
            std::cerr
                << "[CelestialShapeMeshLibrary] failed to load OBJ: "
                << objPath
                << " err="
                << err
                << "\n";

            return false;
        }

        bool boundsInitialized =
            false;

        auto makeVertex =
            [&](const tinyobj::index_t& idx) -> CelestialShapeVertex
            {
                CelestialShapeVertex out;

                if (idx.vertex_index >= 0)
                {
                    const int vi =
                        idx.vertex_index *
                        3;

                    out.pos =
                        glm::vec3(
                            attrib.vertices[vi + 0],
                            attrib.vertices[vi + 1],
                            attrib.vertices[vi + 2]
                        );
                }

                if (idx.texcoord_index >= 0 &&
                    !attrib.texcoords.empty())
                {
                    const int ti =
                        idx.texcoord_index *
                        2;

                    const float u =
                        attrib.texcoords[ti + 0];

                    const float v =
                        attrib.texcoords[ti + 1];

                    out.uv =
                        glm::vec2(
                            u,
                            flipTexcoordV
                                ? 1.0f - v
                                : v
                        );
                }
                else
                {
                    out.uv =
                        glm::vec2(0.0f, 0.0f);
                }

                if (!boundsInitialized)
                {
                    outMesh.minBounds =
                        out.pos;

                    outMesh.maxBounds =
                        out.pos;

                    boundsInitialized =
                        true;
                }
                else
                {
                    outMesh.minBounds =
                        glm::min(
                            outMesh.minBounds,
                            out.pos
                        );

                    outMesh.maxBounds =
                        glm::max(
                            outMesh.maxBounds,
                            out.pos
                        );
                }

                return out;
            };

        for (const tinyobj::shape_t& shape : shapes)
        {
            std::size_t indexOffset =
                0;

            for (std::size_t f = 0;
                 f < shape.mesh.num_face_vertices.size();
                 ++f)
            {
                const int fv =
                    shape.mesh.num_face_vertices[f];

                if (fv < 3)
                {
                    indexOffset +=
                        static_cast<std::size_t>(
                            fv
                        );

                    continue;
                }

                const tinyobj::index_t i0 =
                    shape.mesh.indices[indexOffset + 0];

                for (int k = 1;
                     k < fv - 1;
                     ++k)
                {
                    const tinyobj::index_t i1 =
                        shape.mesh.indices[indexOffset + k];

                    const tinyobj::index_t i2 =
                        shape.mesh.indices[indexOffset + k + 1];

                    CelestialShapeTriangle tri;

                    tri.a =
                        makeVertex(
                            i0
                        );

                    tri.b =
                        makeVertex(
                            i1
                        );

                    tri.c =
                        makeVertex(
                            i2
                        );

                    outMesh.triangles.push_back(
                        tri
                    );
                }

                indexOffset +=
                    static_cast<std::size_t>(
                        fv
                    );
            }
        }

        if (outMesh.triangles.empty() ||
            !boundsInitialized)
        {
            std::cerr
                << "[CelestialShapeMeshLibrary] OBJ has no triangles: "
                << objPath
                << "\n";

            return false;
        }

        outMesh.boundCenter =
            (
                outMesh.minBounds +
                outMesh.maxBounds
            ) *
            0.5f;

        outMesh.boundRadius =
            0.0f;

        auto updateRadius =
            [&](const CelestialShapeVertex& v)
            {
                outMesh.boundRadius =
                    std::max(
                        outMesh.boundRadius,
                        glm::length(
                            v.pos -
                            outMesh.boundCenter
                        )
                    );
            };

        for (const CelestialShapeTriangle& tri : outMesh.triangles)
        {
            updateRadius(
                tri.a
            );

            updateRadius(
                tri.b
            );

            updateRadius(
                tri.c
            );
        }

        if (outMesh.boundRadius <= 0.000001f)
        {
            std::cerr
                << "[CelestialShapeMeshLibrary] OBJ has invalid bounds: "
                << objPath
                << "\n";

            return false;
        }

        return true;
    }

    GLuint CelestialShapeMeshLibrary::loadAlbedoTexture(
        const std::string& albedoPath
    )
    {
        if (albedoPath.empty())
            return 0;

        const std::filesystem::path resolved =
            std::filesystem::path(albedoPath).lexically_normal();

        if (!std::filesystem::exists(resolved))
            return 0;

        const std::string key =
            resolved.generic_string();

        auto existing =
            m_textureByPath.find(
                key
            );

        if (existing != m_textureByPath.end())
            return existing->second;

        auto attempted =
            m_textureLoadAttemptedByPath.find(
                key
            );

        if (attempted != m_textureLoadAttemptedByPath.end() &&
            attempted->second)
        {
            return 0;
        }

        m_textureLoadAttemptedByPath[key] =
            true;

        const GLuint tex =
            TextureLoader::load2D(
                key,
                false
            );

        if (tex != 0)
        {
            GLint oldTexture =
                0;

            glGetIntegerv(
                GL_TEXTURE_BINDING_2D,
                &oldTexture
            );

            glBindTexture(
                GL_TEXTURE_2D,
                tex
            );

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_S,
                GL_CLAMP_TO_EDGE
            );

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_T,
                GL_CLAMP_TO_EDGE
            );

            glBindTexture(
                GL_TEXTURE_2D,
                static_cast<GLuint>(
                    oldTexture
                )
            );
        }

        m_textureByPath[key] = tex;

        if (tex == 0)
        {
            std::cerr
                << "[CelestialShapeMeshLibrary] failed to load albedo: "
                << key
                << "\n";
        }

        return tex;
    }
}