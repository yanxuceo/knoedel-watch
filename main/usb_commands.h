#pragma once
// Settings sent to the watch over USB, one command per line, each answered by
// one line starting with "OK" or "ERR" (tools/watch_config.py, and the
// installer site's settings page, web/settings.html). Commands are not echoed,
// so a key sent stays off the log.
//
//   info                             OK {"version", "battery", "usb", "notes", "waiting",
//                                    "used_kb", "total_kb", "key" (hint or ""), "mode"}
//   wifi add <ssid><TAB><password>   save a network (empty password: open network)
//   wifi list                        OK ["name", ...]: the saved networks
//   wifi remove <ssid>               forget one
//   wifi scan                        OK [{"ssid", "rssi", "secure"}, ...]: the networks around
//   wifi clear                       forget them all
//   key set <key>                    the OpenAI API key
//   key show                         "sk-..." and its last four characters
//   key test                         go online and try the key with a tiny request
//   words get | words set "<JSON string>"   the personal words, one a line
//   mode get | mode set saving|now   how memos are sent (notes_sync.h)
//   net test                         go online, set the clock, go off again
//   notes list                       one "NOTE <json>" line per note, newest first
//   notes sync                       send the memos waiting
//   notes clear                      delete all notes (not the memos waiting)

namespace usb_commands {

// Starts the task that reads the commands.
void start();

}  // namespace usb_commands
