#include "camera.h"
#include "math.h"


namespace TA3D
{

    Camera* Camera::inGame = NULL;



    Camera::Camera()					// Initialise la caméra
        :pos(0.0f, 0.0f, 30.0f)
    {
        widthFactor = 1.0f;

        shakeVector.x = shakeVector.y = shakeVector.z = 0.0f;
        shakeMagnitude = 0.0f;
        shakeDuration = 0.0f;
        shakeTotalDuration = 1.0f;
        rpos = pos;
        dir = up = pos;
        dir.z=-1.0f;					// direction
        up.y=1.0f;						// Haut
        zfar=140000.0f;
        znear=1.0f;
        side = dir * up;
        zfar2 = zfar * zfar;
        mirror = false;
        mirrorPos = 0.0f;
    }

    void Camera::setWidthFactor(const int w, const int h)
    {
        // 1280x1024 is a 4/3 mode
        widthFactor = (w * 4 == h * 5)
            ? 1.0f
            : w * 0.75f / h;
    }

    void Camera::setMatrix(const MATRIX_4x4& v)
    {
        dir.x = dir.y = dir.z = 0.0f;
        up = dir;
        dir.z = -1.0f;
        up.y = 1.0f;
        dir = dir * v;
        up = up * v;
        side = dir * up;
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
                VECTOR rand_vec( ((TA3D_RAND() % 2001) - 1000) * 0.001f * shakeMagnitude,
                                 ((TA3D_RAND() % 2001) - 1000) * 0.001f * shakeMagnitude,
                                 ((TA3D_RAND() % 2001) - 1000) * 0.001f * shakeMagnitude );
                shakeVector += -rdt * 10.0f * shakeVector;
                shakeVector += rand_vec;
                if( shakeVector.x < -20.0f )		shakeVector.x = -20.0f;
                else if( shakeVector.x > 20.0f)	shakeVector.x = 20.0f;
                if( shakeVector.y < -20.0f )		shakeVector.y = -20.0f;
                else if( shakeVector.y > 20.0f)	shakeVector.y = 20.0f;
                if( shakeVector.z < -20.0f )		shakeVector.z = -20.0f;
                else if( shakeVector.z > 20.0f)	shakeVector.z = 20.0f;
            }
        }
        else
        {
            float dt_step = 0.03f;
            for( float c_dt = 0.0f ; c_dt < dt ; c_dt += dt_step )
            {
                float rdt = Math::Min(dt_step, dt - c_dt);
                shakeVector += -rdt * 10.0f * shakeVector;
                if( shakeVector.x < -20.0f )		shakeVector.x = -20.0f;
                else if( shakeVector.x > 20.0f)	shakeVector.x = 20.0f;
                if( shakeVector.y < -20.0f )		shakeVector.y = -20.0f;
                else if( shakeVector.y > 20.0f)	shakeVector.y = 20.0f;
                if( shakeVector.z < -20.0f )		shakeVector.z = -20.0f;
                else if( shakeVector.z > 20.0f)	shakeVector.z = 20.0f;
            }
        }
    }



    void Camera::setView()				// Remplace la caméra OpenGl par celle-ci
    {
        zfar2 = zfar * zfar;

        glMatrixMode (GL_PROJECTION);
        glLoadIdentity ();
        glFrustum(-widthFactor * znear, widthFactor * znear, -0.75f * znear, 0.75f * znear, znear, zfar);
        glMatrixMode(GL_MODELVIEW);

        glLoadIdentity();

        pos = rpos;
        VECTOR FP(pos);
        FP += dir;
        FP += shakeVector;
        gluLookAt(pos.x + shakeVector.x, pos.y + shakeVector.y, pos.z + shakeVector.z,
                  FP.x, FP.y, FP.z,
                  up.x, up.y, up.z);
        if(mirror)
        {
            glScalef(1.0f,-1.0f,1.0f);
            glTranslatef( 0.0f, mirrorPos - 2.0f * shakeVector.y, 0.0f);
        }
    }




} // namespace TA3D
