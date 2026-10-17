# Curriculum

What we teach, in which order, and how we keep it correct and free of repeats. Companion to `FORMAT.md` (how a deck is written down).

## Scope and order

| | Level | Decks (estimate) | Why this order |
|---|---|---|---|
| Season 1 | **A2** | ~35 | Largest audience: people living in German-speaking countries, stuck between A2 and B1. Earlier episodes were already at this level. |
| Season 2 | A1 | ~35 | Fills the base. Many A1 words already appear in A2 example sentences. |
| Season 3 | B1 | ~55 | Builds on both. |

B2 and above are out of scope: their vocabulary is largely abstract and needs a different format.

A deck is **topic-complete at its level**: an A2 deck on the body may include a few A1 words where the topic needs them. Each card records its own level.

## Sources and rules

- **Selection and level**: the Goethe-Institut word lists (A1, A2, B1) are a private checklist only. They are copyrighted (the B1 list requires written consent for reproduction and network distribution), so they are never copied into the public repository, and no example sentence is taken from them.
- **Facts** (gender, plural, Perfekt auxiliary, Präteritum, case government): checked against Duden or DWDS for every card.
- **Everything else is our own work**: example sentences, translations, tips.
- **Variety**: German Standard German (Germany). Austrian and Swiss alternatives go into `tip` or `notes` where they matter.

## Example sentences per level

A deck's example sentences use vocabulary at or below the deck's level, and only these structures:

| Level | Allowed | Avoid |
|---|---|---|
| A1 | main clauses, questions, Präsens, modal verbs, Akkusativ, imperative | subordinate clauses |
| A2 | + Perfekt, Präteritum of sein/haben/modals, Dativ, reflexive verbs, weil / dass / wenn / ob, comparative, *würde/hätte/könnte* for politeness | relative clauses, Passiv, Konjunktiv II for unreal situations, Plusquamperfekt |
| B1 | + relative clauses, Passiv (Präsens), Konjunktiv II, als / nachdem / obwohl / damit, um … zu, Präteritum narration | |

## Taxonomy

Every deck is one **level × topic × kind**. Deck id: `<level>-<topic>-<subject>`, e.g. `a2-gesundheit-zustaende`.

**Topics** (14): person, feelings-character, body-health, daily-routine, home, food, shopping, work, appointments-services (Termin, Behörde, Bank, Post), travel-transport, leisure, nature-weather, time-calendar, communication-media.

**Kinds**:

| Kind | Content | Visual |
|---|---|---|
| Adjective decks | feelings, physical states, character | Knödel |
| Verb decks | actions | Brezel (the ginger cat, id `katze`, with props) |
| Noun decks | objects, places, word groups (numbers, colours, days) | `dinge`: the thing alone on a gender-coloured disc; the cat on some cards |
| Expression decks (Redemittel) | reactions and phrases for situations | Knödel or Katze acting the situation |
| Grammar animations | one rule per deck (Wechselpräpositionen, *mich* or *mir*, verb to the end) | web and social media first; device later |

Only three visual systems (Knödel, Brezel, Dinge), so the firmware has three renderers to maintain, not one per episode.

## No repeats

`curriculum/inventory.csv` lists every word and expression already taught, including the five web episodes before the device existed. Rules:

- A lemma is a **card** exactly once in the whole curriculum. It may appear in example sentences anywhere.
- Rows with role `expression` were taught inside sentences or videos. Making one a card is allowed but reported.
- Before writing a deck: `python3 tools/inventory.py check content/<deck>`. When the deck is final: `python3 tools/inventory.py add content/<deck>`.
- `python3 tools/inventory.py report` shows totals and duplicates. It currently reports one: *aufstehen* is a card in both web episodes `koerperbewegungen` and `morgenroutine`. When those are rebuilt as device decks, only one keeps it.

## Quality checklist (before a deck goes to review)

1. Every card's facts against Duden or DWDS: gender and plural, Perfekt auxiliary, Präteritum, case after verbs, adjectives and prepositions.
2. Every example sentence: word order (verb second, verb last in subordinate clauses, separable prefix), case endings, and that it only uses structures allowed at the level.
3. The sentence is something people actually say, not textbook German.
4. Translations: natural in each language, same meaning, no added information.
5. The collocation on face 2 appears in the example. If the example uses a different construction (state vs. event, adjective vs. verb, *hungrig sein* vs. *Hunger haben*), the card does not show both side by side without saying how they relate: the other one goes into the tip.
6. Tips: true, and written for that language's speakers.
7. `tools/check_deck.py` and `tools/inventory.py check` both clean.
8. Then the teacher review; only then paid audio.

## Season 1 (A2): first 12 decks

Ordered so the device always has something to play. Phase A uses what the firmware already draws; phases B and C wait for the new renderers.

| # | Deck | Kind | Needs | Candidate words (final choice when writing) |
|---|---|---|---|---|
| 0 | `gefuehle` | adjectives | ready | done (Ep.4, A2–B1) |
| 1 | `a2-gesundheit-zustaende` | adjectives | Knödel | müde, wach, hungrig, durstig, satt, krank, gesund, erkältet, fit, *mir ist* kalt / warm / schlecht |
| 2 | `a2-charakter` | adjectives | Knödel | freundlich, nett, lustig, ernst, ruhig, laut, fleißig, faul, ordentlich, pünktlich, neugierig, ehrlich |
| 3 | `a2-reaktionen` | expressions | Knödel | draft 0.1.0: Wie bitte?, Ach so!, Genau!, Echt?, Keine Ahnung, Schade!, Macht nichts, Gute Besserung!, Kein Problem, Gern geschehen, Viel Glück!, Herzlichen Glückwunsch! (mini dialogues; tips on register and mix-ups) |
| 4 | `a2-haushalt-verben` | verbs | Katze | draft 0.1.0: das Bett machen, aufräumen, putzen, staubsaugen, den Boden wischen, Wäsche waschen, Wäsche aufhängen, bügeln, Geschirr spülen, abtrocknen, den Müll rausbringen, die Blumen gießen |
| 5 | `a2-unterwegs-verben` | verbs | Katze | draft 0.1.0: Fahrrad fahren, sich beeilen, verpassen, warten, einsteigen, abfahren, umsteigen, aussteigen, sich verlaufen, ankommen, abholen, mitnehmen (theme: Perfekt with sein vs haben) |
| 6 | `a2-einkaufen-verben` | verbs | Katze | draft 0.1.0: aussuchen, anprobieren, passen, stehen, gefallen, bezahlen, einpacken, bestellen, umtauschen, zurückgeben, ausgeben, sparen (theme: verbs with a dative person) |
| 7 | `a2-termine-verben` | verbs | Katze | draft 0.1.0: anrufen, zurückrufen, einen Termin vereinbaren, bestätigen, verschieben, absagen, sich verspäten, vorbeikommen, sich anmelden, ausfüllen, unterschreiben, abgeben (theme: separable vs inseparable prefixes) |
| 8 | `a2-morgen-verben` | verbs | Katze | rebuild of web Ep.5 at A2, without the *aufstehen* duplicate |
| 9 | `a2-wohnung-nomen` | nouns | dinge | draft 0.1.0: Sofa, Kissen, Teppich, Lampe, Regal, Schrank, Spiegel, Kühlschrank, Herd, Waschmaschine, Heizung, Steckdose (thread: Wo? + Dativ; gender and plural rules) |
| 10 | `a2-gesundheit-nomen` | nouns | dinge | draft 0.1.0: Arzt, Praxis, Krankenkasse, Fieber, Husten, Schnupfen, Rücken, Pflaster, Rezept, Apotheke, Medikament, Tablette |
| 11 | `a2-wetter` | nouns + *es*-sentences | illustration | rebuild of web Ep.3 at A2 |
| 12 | `sich-mich-mir` | grammar | web first | *Ich ziehe mich an* or *Ich ziehe mir den Pullover an*: the question a viewer asked about Ep.5 |

What this asks of the firmware, in order: the Katze renderer and props (before deck 4), an illustration card type plus an `art` field in the format (before deck 9).
