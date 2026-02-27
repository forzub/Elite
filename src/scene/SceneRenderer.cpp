#include "SceneRenderer.h"
#include <glad/gl.h>
#include <iostream>

#include "render/DebugGrid.h"

void SceneRenderer::render(const ClientWorldState& world,
                           EntityId playerId)
{
    const auto& ships = world.ships();

    auto itPlayer = ships.find(playerId.value);
    if (itPlayer == ships.end())
        return;

    const glm::vec3 playerPos =
        itPlayer->second.renderTransform.position;






    for (const auto& pair : ships)
    {
        if (pair.first == playerId.value)
            continue;

        const auto& ship = pair.second;
        const auto& t    = ship.renderTransform;




        glPushMatrix();

        glTranslatef(t.position.x,
                     t.position.y,
                     t.position.z);

        glMultMatrixf(&t.orientation[0][0]);


            float axisLen = 40.0f;

            glLineWidth(3.0f);
            glBegin(GL_LINES);

            // X — красная
            glColor3f(1,0,0);
            glVertex3f(0,0,0);
            glVertex3f(axisLen,0,0);

            // Y — зелёная
            glColor3f(0,1,0);
            glVertex3f(0,0,0);
            glVertex3f(0,axisLen,0);

            // Z — синяя
            glColor3f(0,0,1);
            glVertex3f(0,0,0);
            glVertex3f(0,0,axisLen);

            glEnd();
            glLineWidth(1.0f);





glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

glColor4f(0.2f, 0.6f, 1.0f, 0.15f);

float planeSize = 20.0f;

glBegin(GL_QUADS);
glVertex3f(-planeSize, 0, -planeSize);
glVertex3f( planeSize, 0, -planeSize);
glVertex3f( planeSize, 0,  planeSize);
glVertex3f(-planeSize, 0,  planeSize);
glEnd();












        float s = 5.0f;

        glColor3f(1,0,0);

        glBegin(GL_QUADS);

        // front
        glVertex3f(-s,-s, s);
        glVertex3f( s,-s, s);
        glVertex3f( s, s, s);
        glVertex3f(-s, s, s);

        // back
        glVertex3f(-s,-s,-s);
        glVertex3f( s,-s,-s);
        glVertex3f( s, s,-s);
        glVertex3f(-s, s,-s);

        glEnd();

        glPopMatrix();
    }



   














    auto it  = ships.find(playerId.value);




    if (it != ships.end())
    {
        const auto& t = it->second.renderTransform;

        glPushMatrix();

        glTranslatef(t.position.x,
                    t.position.y,
                    t.position.z);

        glMultMatrixf(&t.orientation[0][0]);



        // 🔥 ДОБАВИТЬ ЭТО
        float forwardOffset = -20.0f;   // вперёд от корабля
        float downOffset    = -20.0f;  // немного вниз
        float sideOffset = 60.0f;   // расстояние от центра вправо

        glTranslatef(0.0f, downOffset, forwardOffset);







        
        float depth = 300.0f;

        glLineWidth(2.0f);
        glColor3f(1,1,1);

        glBegin(GL_LINES);
        glVertex3f(0,0,0);
        glVertex3f(0,0,depth);
        glEnd();

        glLineWidth(1.0f);


        glColor3f(0.7f, 0.7f, 0.7f);

        glBegin(GL_LINES);
        glVertex3f(-100.0f, 0, 0);
        glVertex3f( 100.0f, 0, 0);
        glEnd();


        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glColor4f(0.3f, 0.8f, 1.0f, 0.25f);




        float gridWidth  = 120.0f;
        float gridDepth  = 300.0f;
        float depthStep  = 25.0f;
        float widthStep  = 20.0f;




        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.3f, 0.8f, 1.0f, 0.25f);

        glBegin(GL_LINES);

        // Линии глубины (вперёд = −Z)
        for (float x = -gridWidth; x <= gridWidth; x += widthStep)
        {
            glVertex3f(x, 0, 0);
            glVertex3f(x * 0.2f, 0, -gridDepth);
        }

        // Горизонтальные срезы
        for (float z = depthStep; z <= gridDepth; z += depthStep)
        {
            float scale = 1.0f - (z / gridDepth);

            glVertex3f(-gridWidth * scale, 0, -z);
            glVertex3f( gridWidth * scale, 0, -z);
        }

        glEnd();




        glColor4f(1.0f, 0.5f, 0.2f, 0.15f);

        glBegin(GL_LINES);

        for (float y = -gridWidth; y <= gridWidth; y += widthStep)
        {
            glVertex3f(sideOffset, y, 0);
            glVertex3f(sideOffset * 0.2f, y * 0.2f, -gridDepth);
        }

        glEnd();







        glPopMatrix();
    }


}