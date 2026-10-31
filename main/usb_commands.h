#pragma once
// Settings sent to the watch over USB, one command per line, each answered by
// one line starting with "OK" or "ERR" (tools/watch_config.py; later the
// installer page). Commands are not echoed, so a key sent stays off the log.
//
//   wifi add <ssid><TAB><password>   save a network (empty password: open network)
//   wifi list                        the saved networks' names
//   wifi clear                       forget them all
//   key set <key>                    the OpenAI API key
//   key show                         "sk-..." and its last four characters
//   net test                         go online, set the clock, go off again
//   notes list                       one "NOTE <json>" line per note, newest first
//   notes sync                       send the memos waiting
//   notes clear                      delete all notes (not the memos waiting)

namespace usb_commands {

// Starts the task that reads the commands.
void start();

}  // namespace usb_commands
