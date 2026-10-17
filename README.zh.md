# Knödel Watch

[![build](https://github.com/yanxuceo/knoedel-watch/actions/workflows/build.yml/badge.svg)](https://github.com/yanxuceo/knoedel-watch/actions/workflows/build.yml)

[English](README.md) | 中文

**可爱的小角色把每个单词演出来，在一块小小的圆屏上学外语。**

> 🇩🇪 第一个版本教的是**德语**（A2 级别），配有英文和中文翻译，这套课程叫 *Deutsch mit Knödel*。以后可以加入更多语言：卡组格式并不限于德语。

<p align="center"><img src="docs/img/hero.gif" width="798" alt="三块手表：Knödel 撒彩纸庆祝，Brezel 在吸尘，Brezel 在骑车"></p>

运行在 M5Stack StopWatch Dev Kit（ESP32-S3，圆形 AMOLED 触摸屏，带扬声器和运动传感器）上的动画单词卡片。每个词都由两个角色表演出来：团子 **Knödel** 负责形容词和日常表达，橘猫 **Brezel** 负责动词。名词直接画出东西本身，底色是它的词性颜色。

每张卡片最多三面：单词和它的动画；语法行、例句和翻译；学习提示。所有德语内容都有配音，用的是合成语音（Microsoft Azure 神经网络语音合成，不是真人录音），一问一答的小对话由两个声音朗读。除了卡片，手表上还有按间隔重复安排的练习模式、带每日一词的时钟和番茄钟。

> **关于内容。** 卡片内容由 AI 编写，尚未经母语者校对。发现错误请提 issue，我们会在下一个内容包里修正。

## 认识 Knödel 和 Brezel

单词不只是显示出来，而是被演出来。每张卡片都有自己的小场景，由数据在手表上实时绘制：姿势、动作、特效、道具和想法泡泡。

**Knödel** 是一个巴伐利亚团子，负责情绪、状态和日常反应：它会笑、会生气、会打哈欠、会打喷嚏、会脸红，还会在泡泡里想念它惦记的东西。

<p align="center"><img src="docs/img/knoedel.png" width="974" alt="Knödel 的八张卡片：glücklich、traurig、sauer、überrascht、müde、erkältet、lustig、Herzlichen Glückwunsch"></p>

**Brezel** 是一只橘猫，负责动词，还会带上道具：吸尘器、浇水壶、自行车、雨中的伞、开走了没等她的公交车、刷卡机、存钱猪、诊所的门。

<p align="center"><img src="docs/img/brezel.png" width="974" alt="Brezel 的八张卡片：staubsaugen、Blumen gießen、Fahrrad fahren、mitnehmen、verpassen、bezahlen、sparen、vorbeikommen"></p>

卡片之外：两个声音朗读的对话例句，只看表演来出题、答对了还会为你庆祝的练习模式，带每日一词的时钟，以及番茄钟。

<p align="center"><img src="docs/img/screens.png" width="1210" alt="对话例句、练习题、庆祝画面、时钟、番茄钟、主菜单里的时钟"></p>

## 硬件

一块 [M5Stack StopWatch Dev Kit](https://openelab.io/products/m5stack-stopwatch-dev-kit-esp32-s3?sca_ref=12520936.Zn0RxZ6jdFG) 和一根 USB-C 线，不需要别的东西。

上面是推荐链接：通过它购买，本项目作者会获得佣金，你付的价格不变。这个套件在 M5Stack 官方和其他店铺也能买到。

### 其他开发板

固件只通过一层很薄的板级接口 [`main/board/board.h`](main/board/board.h) 访问硬件，所以能运行在其他 ESP32-S3 开发板上，条件是有 460～480 像素左右的圆形触摸屏、两个按键和扬声器。目前支持：

| 开发板 | 状态 |
|---|---|
| M5Stack StopWatch Dev Kit | 主力板子，每次发布前都会测试 |
| [微雪 ESP32-S3-Touch-AMOLED-1.75C](docs/boards/waveshare-175c.md) | 由 [@pockegar](https://github.com/pockegar) 移植和维护，有空时会在自己的板子上测试 |

想移植到你的板子上，请看 [docs/PORTING.md](docs/PORTING.md)（英文）。

## 安装

不需要任何开发工具：在电脑上用 Chrome 或 Edge 打开 **[网页安装器](https://yanxuceo.github.io/knoedel-watch/)**，用能传数据的 USB-C 线连上手表，点"安装"。第一次安装时选择清空手表；以后更新时选择保留数据，练习进度不会丢。

习惯用命令行？每个 [Release](https://github.com/yanxuceo/knoedel-watch/releases) 里也有一个包含全部内容的镜像：先 `pip install esptool`，然后运行 `python3 -m esptool --chip esp32s3 write-flash 0x0 knoedel-watch-<版本>-full.bin`（相当于全新安装，会清空练习进度和设置）。

想自己编译，见 [编译和烧录](#编译和烧录)。

## 第一次开机

手表默认是英文界面。切换成中文：*Settings* → *Language*，轻触一下。然后设置时间：*设置* → *时间*，轻触开始，依次调整时、分、日、月、年（按键改数值，轻触跳到下一项，最后一下保存）。不设时间也能学习，只有时钟页面需要它。

## 主菜单

| | |
|---|---|
| **时钟** | 时间和每日一词 |
| **学德语** | 各个卡组，逐张学习 |
| **练习** | 把学过的卡片拿出来再考你几张 |
| **番茄钟** | 专注 25 分钟，休息 5 分钟 |
| **设置** | 语言、时间、音量、亮度、关机 |

菜单文字跟随设置里选的语言。手表会记住你上次的位置。

## 操作

| | |
|---|---|
| KEY B（蓝色，右）或向左滑 | 下一个 |
| KEY A（黄色，左）或向右滑 | 上一个 |
| 轻触屏幕 | 进入、翻到卡片的下一面、更改设置 |
| 长按 KEY A | 返回 |

运动传感器带来三个手势：**拿起手表**，变暗的屏幕会亮起来；**把手表扣过来**，屏幕熄灭（在番茄钟页面还会开始计时）；在时钟页面**摇一摇**，换一个词。

用电池时，1 分钟不操作屏幕变暗，5 分钟不操作自动关机。插着 USB 时不会关机，闲置时自动回到时钟页面。

## 学德语

主菜单 → *学德语* → 选一个卡组 → 逐张学习。轻触翻面：

1. 角色表演这个词，下面是单词和释义；
2. 语法行（过去式和完成时、复数，或带格的句型），以及例句和翻译；
3. 学习提示。

## 练习

主菜单 → *练习* 会从你学过的卡片中挑几张来考你，每次一两分钟。

1. 角色把这个词表演出来，不显示文字。两秒后，释义作为提示出现。
2. 在心里说出德语词，然后按任意键或轻触屏幕，查看并听到答案。
3. KEY B（蓝色，✓）：记得。KEY A（黄色，✗）：没记住，这张卡会在本轮结束前再出现一次。

连续答对 3 个会有一段小小的庆祝。记得的词会在 1、2、4 … 64 天后再出现，没记住的词从头开始。没有需要"还债"的积压，也没有要设置的东西。

## 时钟

时间、日期和每日一词：角色先把这个词表演出来，然后单词和释义出现在同一位置，两者轮流显示。轻触听发音；按键或摇一摇换一个词。外圈的刻度显示秒数。

## 番茄钟

屏幕外圈的圆环随剩余时间缩短：专注时是红色，休息时是绿色。专注时 Brezel 陪你一起写，休息时 Knödel 哼着歌。轻触开始或暂停，KEY B 跳到下一阶段，KEY A 重新开始当前阶段。也可以直接把手表扣在桌上：计时开始，屏幕熄灭。

一个阶段结束时，手表会响一段提示音，大字显示德语 *Pause!*（该休息了）或 *Weiter!*（继续），即使你已经离开番茄钟页面也会提醒。轻触进入下一阶段。

## 手表里有什么

程序和学习内容是分开的：

| | 程序（固件） | 内容包 |
|---|---|---|
| 包含 | 菜单、角色、时钟、番茄钟、音频播放 | 卡组、字体、音频 |
| 何时变化 | 代码改动时 | 增加或修正卡组时 |
| 写入方式 | `idf.py flash` | `python3 tools/flash_content.py` |

所以更新内容不需要更新固件，一块手表从一个内容包换到另一个（比如从 A2 换到 B1）不到一分钟。

第一个内容包 `a2-start` 有 15 个卡组，每组 12 张，共 180 张，A2 级别（其中两个卡组带一些 B1 词汇）。翻译有英文和中文。

| 卡组 | 主题 | 类型 | 例子 | 级别 | 谁来演 |
|---|---|---|---|---|---|
| Wie fühlst du dich? | 情绪 | 形容词 | glücklich、traurig、nervös | A2–B1 | Knödel |
| Wie geht's dir? | 身体状态 | 形容词 | müde、hungrig、durstig | A2 | Knödel |
| Wie bist du? | 性格 | 形容词 | fleißig、faul、ordentlich | A2 | Knödel |
| Hausarbeit | 做家务 | 动词 | aufräumen、putzen、staubsaugen | A2 | Brezel |
| Unterwegs | 出门在外 | 动词 | Fahrrad fahren、sich beeilen、verpassen | A2 | Brezel |
| Reaktionen | 日常反应 | 短语 | Wie bitte?、Ach so!、Echt? | A2 | Knödel |
| Einkaufen | 去购物 | 动词 | aussuchen、anprobieren、passen | A2 | Brezel |
| Termine | 预约和办事 | 动词 | anrufen、zurückrufen、bestätigen | A2 | Brezel |
| Meine Wohnung | 家里的东西 | 名词 | das Sofa、der Teppich、die Lampe | A2 | 东西本身 |
| Beim Arzt | 看病 | 名词 | der Arzt、die Praxis、das Fieber | A2 | 东西本身 |
| In den Bergen | 在山里 | 名词 | der Berg、der Rucksack、die Hütte | A2 | 东西本身 |
| Wir waren wandern! | 去徒步啦 | 动词 | packen、wandern、klettern | A2 | Brezel 和 Knödel |
| Auf der Hütte | 山间小屋 | 短语 | Grüß Gott!、Prost!、Zahlen, bitte! | A2 | Brezel 和 Knödel |
| Im Fitnessstudio | 健身房 | 动词 | sich aufwärmen、trainieren、Gewichte heben | A2–B1 | Brezel 和 Knödel |
| Sport im Freien | 户外运动 | 动词 | joggen、Fußball spielen、Ski fahren | A2–B1 | Brezel 和 Knödel |

最后五个卡组是两个小系列：一个是去多洛米蒂徒步（那里说德语，Knödel 就是当地的特色菜），一个是运动双集，Knödel 当教练。

**[在浏览器里看看所有卡片](https://yanxuceo.github.io/knoedel-watch/decks/)**：点一下卡片就能翻面。

**更多卡组正在制作中。** 新卡组会放进新的内容包里，和固件一样，在[网页安装器](https://yanxuceo.github.io/knoedel-watch/)上安装。想学什么主题？欢迎[提 issue](https://github.com/yanxuceo/knoedel-watch/issues)。

## 编译和烧录

需要 [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) v6.0。生成内容包还需要 Python 3 和 `pip install jsonschema pillow fonttools esptool`，排版检查另外需要 `cmake` 和 SDL2（`brew install sdl2`）。

```bash
# 1. 程序
idf.py -p <端口> flash

# 2. 内容包
python3 tools/import_decks.py          # 从 content/ 生成 packs/a2-start.bin
python3 tools/flash_content.py         # 写入手表
```

没有内容包的手表会显示 "No content"，等待第 2 步。

`import_decks.py` 会检查每个卡组，用固件自己的绘制代码排一遍所有语言的所有文字，圆屏上放不下的内容包会被拒绝。没有 cmake 或 SDL2 时可以加 `--skip-fit` 跳过这项检查。加 `--locale zh-Hans` 可以让新手表默认显示中文。

### 发布新版本

```bash
idf.py build && python3 tools/import_decks.py && python3 tools/make_release.py
```

`release/site/` 是网页安装器（ESP Web Tools：网页、清单和闪存的四个部分），用 GitHub Pages 发布；`release/knoedel-watch-<版本>-full.bin` 放进 GitHub Release。版本号是 `CMakeLists.txt` 里的 `PROJECT_VER`。发布前想在本地试一下：`python3 -m http.server -d release/site 8000`，然后打开 http://localhost:8000。

### 配音

仓库里的配音由 Azure 语音服务生成。只有在修改或新增德语文字时才需要语音服务：

- `--paid`：用 Azure 生成缺少的音频（环境变量 `SPEECH_KEY` 和 `SPEECH_REGION`）。不加它，导入工具只报告需要生成哪些。
- `--audio edge`：改用免费的 edge-tts（`pip install edge-tts`），试自己的卡片足够用。这类音频不会进入 git。

## 制作自己的卡组

一个卡组就是一个文件夹 `content/<卡组编号>/`，里面有一个 `deck.json`。格式见 [FORMAT.md](FORMAT.md)（英文），用 `tools/check_deck.py` 检查；`tools/build_preview.py` 可以生成带同样动画的网页预览。

`content/library.json` 定义一个内容包：它的名字和包含的卡组。要做另一个内容包，就另写一个清单文件，用 `--library` 传入。

动画是数据：卡片从 `registry/<角色>.json` 里选用姿势、动作、特效、道具和思考气泡。这些名字具体画什么，在 `main/knoedel.cpp`、`main/knoedel_bubbles.cpp` 和 `main/katze.cpp` 里，`tools/preview/*.js` 是画法参考。

## 目录

```
main/          固件（ESP-IDF，M5GFX）
main/board/    各开发板的硬件部分，统一实现 board.h
sim/           界面的电脑版，用于排版检查
tools/         导入、卡组检查、配音、网页预览、烧录
content/       卡组和配音
registry/      每个角色会做什么
curriculum/    后续卡组的规划，以及至今教过的所有词
FORMAT.md      卡组格式
docs/PORTING.md  如何把固件移植到其他开发板
```

## 许可

- 源代码：MIT（[LICENSE](LICENSE)）。
- 学习内容（`content/`、`curriculum/`）：CC BY-NC-SA 4.0（[content/LICENSE](content/LICENSE)）。可以在非商业用途下分享和改编，需署名并以相同许可发布。
- 角色 Knödel 和 Brezel，以及 "Deutsch mit Knödel" 这个名字：保留所有权利。欢迎带着它们编译、运行和修改本项目，但不要把它们用在你自己的品牌、产品或周边上。

详情和第三方组件见 [LICENSES.md](LICENSES.md)。
