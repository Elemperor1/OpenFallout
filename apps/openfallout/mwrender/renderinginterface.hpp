#ifndef GAME_RENDERING_INTERFACE_H
#define GAME_RENDERING_INTERFACE_H

namespace OFRender
{
    class Objects;
    class Actors;

    class RenderingInterface
    {
    public:
        virtual OFRender::Objects& getObjects() = 0;
        virtual ~RenderingInterface() {}
    };
}
#endif
