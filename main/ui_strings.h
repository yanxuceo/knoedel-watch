#pragma once
// Menu and settings text, in the learner's language. tools/import_decks.py
// reads this file so that the locale fonts contain every character used here:
// keep one block per language, named as below.

#include <string.h>

struct UiText {
    const char *learn, *practice, *settings;      // main menu, under the German name
    const char *language, *language_name;         // settings: name of the item, name of this language
    const char *volume, *brightness, *power_off;  // settings
    const char *tap_change, *tap_off, *off;       // hints and values
    const char *well_done, *learn_first;          // practice: after a session, and when no card has been seen yet
    const char *clock, *timer, *time;             // main menu; settings
    const char *edit_hint;                        // settings: while a value is being changed
    const char *focus, *pause, *start_hint, *running_hint, *paused_hint;  // focus timer
    const char *break_now, *back_to_work;         // focus timer: when a phase ends
    const char *in_a_row;                         // practice: "%d" right answers in a row
    const char *weekdays[7], *months[12];         // the clock's date, Sunday first
    // Notes (voice memos): main menu, recording, the notes page
    const char *notes;
    const char *todo, *idea, *memo;               // the kinds of card
    const char *today, *yesterday, *tomorrow;
    const char *let_go_keep, *left_cancel, *seconds_left;  // while recording; "%d"
    const char *kept, *kept_online, *kept_offline, *too_short, *too_short_hint, *cancelled, *cancelled_hint;
    const char *waiting_wifi, *sending, *waiting_n, *online, *offline, *limit_reached, *no_key;  // "%d"
    const char *noted;                            // before the note, in the message when one is made
    const char *no_notes, *no_notes_hint, *this_note, *all_done, *remove, *cancel, *removed, *more_n;  // "%d"
    const char *weekdays_short[7];                // a task's day, Sunday first
    const char *date_short;                       // "%d" month, "%d" day, for a task's day further away
    const char *kept_later;                       // kept, sent with the next ones (saving battery)
    const char *sending_setting, *sending_saving, *sending_at_once;  // settings: how memos are sent
    // What was done: the page after the open tasks, its list, the main menu
    const char *week_done, *tasks_unit, *streak, *see_all, *done_list, *undo, *done_today;  // "%d"
    const char *week_initials[7];                 // Monday first
};

inline const UiText &ui_text(const char *locale)
{
    static const UiText ZH = {"学德语", "练习", "设置", "语言", "中文", "音量", "亮度", "关机", "轻触更改", "轻触关机", "静音",
                              "做得好", "还没有卡片",
                              "时钟", "番茄钟", "时间", "按键调整 · 轻触下一项",
                              "专注", "休息", "轻触开始 · 或把手表扣过来", "轻触暂停", "轻触继续",
                              "该休息一下了", "回来继续专注", "连对 %d 个",
                              {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"},
                              {"1月", "2月", "3月", "4月", "5月", "6月", "7月", "8月", "9月", "10月", "11月", "12月"},
                              "速记",
                              "待办", "想法", "录音",
                              "今天", "昨天", "明天",
                              "松开 保存", "按左键 取消", "还剩 %d 秒",
                              "已保存", "正在转录…", "联网后自动转录", "太短了", "按住说完再松开", "已取消", "这段录音没有保存",
                              "等待联网", "转录中…", "%d 条待转录", "在线", "离线", "本月额度已用完", "还没有设置 API key",
                              "已记下：",
                              "还没有速记", "按住右键 说一句话", "这条速记", "全部完成", "删除", "取消", "已删除", "还有 %d 项",
                              {"周日", "周一", "周二", "周三", "周四", "周五", "周六"},
                              "%d月%d日",
                              "稍后一起转录，更省电",
                              "速记上传", "省电", "立即",
                              "本周完成", "件", "连续 %d 天都有完成", "查看全部", "已完成", "撤销完成", "今天完成 %d 件",
                              {"一", "二", "三", "四", "五", "六", "日"}};
    static const UiText EN = {"Learn German", "Practice", "Settings", "Language", "English", "Volume", "Brightness", "Power off",
                              "Tap to change", "Tap to switch off", "Muted",
                              "Well done", "No cards yet",
                              "Clock", "Focus timer", "Time", "Keys: adjust · Tap: next",
                              "Focus", "Break", "Tap, or turn me face down", "Tap to pause", "Tap to resume",
                              "Time for a break", "Back to focus", "%d in a row",
                              {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"},
                              {"January", "February", "March", "April", "May", "June", "July", "August", "September",
                               "October", "November", "December"},
                              "Notes",
                              "To do", "Idea", "Memo",
                              "Today", "Yesterday", "Tomorrow",
                              "Let go to keep", "Left key: cancel", "%d s left",
                              "Kept", "Transcribing…", "Goes out when online", "Too short", "Hold the key while you talk",
                              "Cancelled", "Nothing was kept",
                              "Waiting for Wi-Fi", "Transcribing…", "%d waiting", "Online", "Offline",
                              "Monthly limit reached", "No API key yet",
                              "Noted: ",
                              "No notes yet", "Hold the right key and talk", "This note", "All done", "Delete", "Cancel",
                              "Deleted", "%d more",
                              {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
                              "%d/%d",
                              "Sent with the next ones",
                              "Notes upload", "Save battery", "At once",
                              "Done this week", "tasks", "%d days in a row", "See all", "Done", "Not done",
                              "%d done today",
                              {"M", "T", "W", "T", "F", "S", "S"}};
    return strncmp(locale, "zh", 2) == 0 ? ZH : EN;
}
