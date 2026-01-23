#include "Core.h"
#include "Application/Context.h"
#include "Application/Interface.h"

namespace Boom
{
    AppContext::~AppContext()
    {
        // Shutdown video system first
        if (videoSystem) {
            videoSystem->Shutdown();
        }

        for (AppInterface*& layer : layers)
        {
            BOOM_DELETE(layer);
            SoundEngine::Instance().Shutdown();
        }
    }
}