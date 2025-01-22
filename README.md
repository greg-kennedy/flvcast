# flvcast
Greg Kennedy, 2025

CLI application for streaming FLV movies to an RTMP URL

## Overview
This is a small tool to broadcast an FLV-container video to an RTMP stream URL, using [librtmp](https://rtmpdump.mplayerhq.hu/).

Generally, streaming expects live data encoded on-the-fly, before sending to the remote service for broadcast.  However, if the content is pre-recorded, it can instead be preprocessed and stored - then later sent directly to the streaming service, without needing to encode or further process the data.  The system requirements to encode video vs uploading it to a streamer are dramatically different: encoding requires computing power, whereas streaming demands network bandwidth and stability.

Thus, this tool could be used to separate the two steps: encoding (say on a strong desktop computer), and broadcasting (say on a colocated Raspberry Pi or small VPS).  This is the ideal case for something like a streaming radio station, or "online TV" which only plays rerecorded content.  Consider it as an extremely lightweight alternative to e.g. "OBS screen-recording a video player" or "FFMPEG repeatedly re-encoding the same videos to an rtmp sink".  Run your own [LoFi Radio: Beats To Sip To](https://wrvu.org/a-corporate-enigma/)!

## Usage
flvcast has three "play modes", which determine the videos to play and the order to play them.  You must choose one mode through CLI arguments.  The stream URL is always the last argument.
```
./flvcast <play-args> <stream-url>
```

The play modes available are:
* `-f <filename>`: Single file mode
  Plays one video file through, then exits.  The video to play is in `filename` argument.  Optional arguments:
  * `-l <loop_count>`: Play the video `loop_count` times before exiting.
* `-p <playlist>`: Playlist mode
  Plays a playlist of videos, then exits.  The playlist is in .m3u format, one video per line.  Comments are supported, but EXTM3U directives are ignored.
  Optional arguments:
  * `-l <loop_count>`: Play the playlist `loop_count` times before exiting.
  * `-s`: Play in shuffled order.  If `loop_count` is also set, the shuffled playlist is completely played before looping, and each iteration reshuffles again.
* `-e <script>`: Script mode
  Executes a script each time a video is queued.  The script must print a single line to `STDOUT`, containing the path to a video to read and play.
  This mode will continue playing as long as the script returns filenames.  To terminate playback, print an empty line instead.

All modes support an optional `-v` argument which triggers more verbose output.

Some examples:

```
# Play site_banner.flv 1000 times and exit
./flvcast -f site_banner.flv -l 1000 rtmp://in.contribute.live-video.net/app/STREAM_KEY

# Shuffle and play the whole playlist, then exit
./flvcast -p music_videos.m3u -s rtmp://in.contribute.live-video.net/app/STREAM_KEY

# Invoke a script to play curated content, with extra printout
./flvcast -v -e auto_director.sh rtmp://in.contribute.live-video.net/app/STREAM_KEY
```

## Input Format
Input videos MUST be in FLV file format.  The FLV container is a small wrapper format that supports a handful of legacy codecs - however, most services support only FLV containing x264 video streams and AAC audio.  You will need to prepare some videos for streaming.

If you have an mp4 file already containing the proper streams, you can reformat it (without re-encoding) using `ffmpeg`.
```
ffmpeg -i input.mp4 -c:v copy -c:a copy output.flv
```

On the other hand, if your file is in some other format, you'll need to re-encode it.  This is potentially time-consuming and needs computer hardware!
```
ffmpeg -i input.mkv -c:v libx264 -c:a libfdk_aac output.flv
```

Video encoding is outside of the scope of this project: please consult ffmpeg documentation for hints on encoding quality and/or hardware encoding features.

## License
flvcast is public-domain software.

librtmp is licensed under the GPLv2, see `COPYING` in the distribution archive.
