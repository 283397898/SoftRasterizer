#pragma once

#ifdef SOFTRENDERER_EXPORTS
    #define SR_API __declspec(dllexport)
#else
    #define SR_API __declspec(dllimport)
#endif
