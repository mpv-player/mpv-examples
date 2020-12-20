/*

 MIT License
 
 Copyright © 2020 Samuel Venable
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 
*/

#include "videoplayer.h"

using std::string;

#ifdef _WIN32 /* Windows */
#define EXPORTED_FUNCTION extern "C" __declspec(dllexport)
#else /* Linux, macOS, and FreeBSD */
#define EXPORTED_FUNCTION extern "C" __attribute__((visibility("default"))) 
#endif

EXPORTED_FUNCTION void splash_set_stop_mouse(bool stop) {
  videoplayer::splash_set_stop_mouse(stop);
}

EXPORTED_FUNCTION void splash_set_stop_key(bool stop) {
  videoplayer::splash_set_stop_key(stop);
}

EXPORTED_FUNCTION void splash_set_window(char *wid) {
  videoplayer::splash_set_window(wid);
}

EXPORTED_FUNCTION void splash_set_volume(int vol) {
  videoplayer::splash_set_volume(vol);
}

EXPORTED_FUNCTION void splash_show_video(char *fname, bool loop) {
  videoplayer::splash_show_video(fname, loop);
}

EXPORTED_FUNCTION char *video_add(char *fname) {
  static string result;
  result = videoplayer::video_add(fname);
  return (char *)result.c_str();
}

EXPORTED_FUNCTION bool video_get_option_was_set(char *ind, char *option) {
  return videoplayer::video_get_option_was_set(ind, option);
}

EXPORTED_FUNCTION char *video_get_option_string(char *ind, char *option) {
  static string result;
  result = videoplayer::video_get_option_string(ind, option);
  return (char *)result.c_str();
}

EXPORTED_FUNCTION void video_set_option_string(char *ind, char *option, char *value) {
  videoplayer::video_set_option_string(ind, option, value);
}

EXPORTED_FUNCTION void video_play(char *ind) {
  videoplayer::video_play(ind);
}

EXPORTED_FUNCTION bool video_is_paused(char *ind) {
  return videoplayer::video_is_paused(ind);
}

EXPORTED_FUNCTION bool video_is_playing(char *ind) {
  return videoplayer::video_is_playing(ind);
}

EXPORTED_FUNCTION int video_get_volume_percent(char *ind) {
  return videoplayer::video_get_volume_percent(ind);
}

EXPORTED_FUNCTION void video_set_volume_percent(char *ind, int volume) {
  videoplayer::video_set_volume_percent(ind, volume);
}

EXPORTED_FUNCTION char *video_get_window_identifier(char *ind) {
  static string result;
  result = videoplayer::video_get_window_identifier(ind);
  return (char *)result.c_str();
}

EXPORTED_FUNCTION void video_set_window_identifier(char *ind, char *wid) {
  videoplayer::video_set_window_identifier(ind, wid);
}

EXPORTED_FUNCTION bool video_exists(char *ind) {
  return videoplayer::video_exists(ind);
}

EXPORTED_FUNCTION void video_pause(char *ind) {
  videoplayer::video_pause(ind);
}

EXPORTED_FUNCTION void video_stop(char *ind) {
  videoplayer::video_stop(ind);
}

EXPORTED_FUNCTION unsigned video_get_width(char *ind) {
  return videoplayer::video_get_width(ind);
}

EXPORTED_FUNCTION unsigned video_get_height(char *ind) {
  return videoplayer::video_get_height(ind);
}

EXPORTED_FUNCTION double video_get_position(char *ind) {
  return videoplayer::video_get_position(ind);
}

EXPORTED_FUNCTION double video_get_duration(char *ind) {
  return videoplayer::video_get_duration(ind);
}

EXPORTED_FUNCTION void video_delete(char *ind) {
  videoplayer::video_delete(ind);
}
