#ifndef OPENFALLOUT_MWSOUND_MOVIEAUDIOFACTORY_H
#define OPENFALLOUT_MWSOUND_MOVIEAUDIOFACTORY_H

#include <osg-ffmpeg-videoplayer/audiofactory.hpp>

namespace OFSound
{

    class MovieAudioFactory : public Video::MovieAudioFactory
    {
        std::unique_ptr<Video::MovieAudioDecoder> createDecoder(Video::VideoState* videoState) override;
    };

}

#endif
