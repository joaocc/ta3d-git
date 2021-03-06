/*  TA3D, a remake of Total Annihilation
    Copyright (C) 2005  Roland BROCHARD

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA*/
#include "../types.h"
#include "camera.h"
#include "math.h"

namespace TA3D
{

    Camera* Camera::inGame = NULL;



    Camera::Camera()					// Initialise la caméra
    {
        reset();
    }

    void Camera::reset()
    {
        // Pos
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 30.0f;

        // Factor
        widthFactor = 1.0f;

        shakeVector.reset();
        shakeMagnitude = 0.0f;
        shakeDuration = 0.0f;
        shakeTotalDuration = 1.0f;
        rpos  = pos;
        dir   = up = Vec();
        dir.z = -1.0f; // direction
        up.y  = 1.0f; // Haut
        zfar  = 140000.0f;
        znear = 1.0f;
        side  = dir ^ up;
        zfar2  = zfar * zfar;
        mirror = false;
        mirrorPos = 0.0f;
        zoomFactor = 0.5f;
    }


    void Camera::setWidthFactor(const int w, const int h)
    {
        widthFactor = w * 0.75f / h;
    }

    void Camera::setMatrix(const Matrix& v)
    {
        dir.reset();
        up = dir;
        dir.z = -1.0f;
        up.y = 1.0f;
        dir = dir * v;
        up = up * v;
        side = dir ^ up;
    }



    void Camera::setShake(const float duration, float magnitude)
    {
        magnitude *= 0.1f;
        if (shakeDuration <= 0.0f || magnitude >= shakeMagnitude)
        {
            shakeDuration = duration;
            shakeTotalDuration = duration + 1.0f;
            shakeMagnitude = magnitude;
        }
    }


    void Camera::updateShake(const float dt)
    {
        if (shakeDuration > 0.0f)
        {
            shakeDuration -= dt;
            float dt_step = 0.03f;
            for (float c_dt = 0.0f ; c_dt < dt ; c_dt += dt_step)
            {
                float rdt = Math::Min(dt_step, dt - c_dt);
                Vector3D rand_vec( ((TA3D_RAND() % 2001) - 1000) * 0.001f * shakeMagnitude,
                                   ((TA3D_RAND() % 2001) - 1000) * 0.001f * shakeMagnitude,
                                   ((TA3D_RAND() % 2001) - 1000) * 0.001f * shakeMagnitude );
                shakeVector += -rdt * 10.0f * shakeVector;
                shakeVector += rand_vec;
                if (shakeVector.x < -20.0f)		shakeVector.x = -20.0f;
                else if(shakeVector.x > 20.0f)	shakeVector.x = 20.0f;
                if (shakeVector.y < -20.0f )	shakeVector.y = -20.0f;
                else if(shakeVector.y > 20.0f)	shakeVector.y = 20.0f;
                if (shakeVector.z < -20.0f)		shakeVector.z = -20.0f;
                else if(shakeVector.z > 20.0f)	shakeVector.z = 20.0f;
            }
        }
        else
        {
            float dt_step = 0.03f;
            for (float c_dt = 0.0f; c_dt < dt; c_dt += dt_step)
            {
                float rdt = Math::Min(dt_step, dt - c_dt);
                shakeVector += -rdt * 10.0f * shakeVector;

                if( shakeVector.x < -20.0f )    shakeVector.x = -20.0f;
                else
                    if( shakeVector.x > 20.0f)	shakeVector.x = 20.0f;
                if (shakeVector.y < -20.0f)     shakeVector.y = -20.0f;
                else
                    if( shakeVector.y > 20.0f)	shakeVector.y = 20.0f;
                if (shakeVector.z < -20.0f)     shakeVector.z = -20.0f;
                else
                    if( shakeVector.z > 20.0f)	shakeVector.z = 20.0f;
            }
        }
    }



    void Camera::setView(bool classic)
    {
        zfar2 = zfar * zfar;

        glMatrixMode (GL_PROJECTION);
        glLoadIdentity ();
        glFrustum(-widthFactor * znear, widthFactor * znear, -0.75f * znear, 0.75f * znear, znear, zfar);

        if (classic)
        {
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
        }

        pos = rpos;
        Vector3D FP(pos);
        FP += dir;
        FP += shakeVector;
        gluLookAt(pos.x + shakeVector.x, pos.y + shakeVector.y, pos.z + shakeVector.z,
                  FP.x, FP.y, FP.z,
                  up.x, up.y, up.z);

        if (!classic)
        {
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
        }

        if (mirror)
        {
            glScalef(1.0f, -1.0f, 1.0f);
            glTranslatef(0.0f, mirrorPos - 2.0f * shakeVector.y, 0.0f);
        }
    }

    QVector<Vector3D> Camera::getFrustum()
    {
        QVector<Vector3D> frustum;
        frustum.push_back( rpos + znear * (-widthFactor * side + 0.75f * up + dir) );
        frustum.push_back( rpos + znear * (widthFactor * side + 0.75f * up + dir) );
        frustum.push_back( rpos + znear * (-widthFactor * side - 0.75f * up + dir) );
        frustum.push_back( rpos + znear * (widthFactor * side - 0.75f * up + dir) );

        frustum.push_back( rpos + zfar * (-widthFactor * side + 0.75f * up + dir) );
        frustum.push_back( rpos + zfar * (widthFactor * side + 0.75f * up + dir) );
        frustum.push_back( rpos + zfar * (-widthFactor * side - 0.75f * up + dir) );
        frustum.push_back( rpos + zfar * (widthFactor * side - 0.75f * up + dir) );

        return frustum;
    }

    Vec Camera::getScreenVector(float x, float y)
    {
        return znear * (widthFactor * (2.0f * x - 1.0f) * side + 0.75f * (1.0f - 2.0f * y) * up + dir);
    }

    void Camera::setScreenCoordinates(int w, int h)
    {
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }
} // namespace TA3D
