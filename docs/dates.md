# Dates and durations

What the `date` and `duration` option types understand. Anything not on this page is rejected. The parser never
guesses.

- A `date` is a point in time: `tomorrow 3pm`, `3 days ago`, `2026-10-06 14:00`.
- A `duration` is a length of time: `90 seconds`, `for 2 weeks`.

The grammar is a subset of the Perl module DateTime::Format::Natural, which Getopt::Pad uses. Where both understand a
text, both give the same result, except for the [differences](#differences-from-datetimeformatnatural) at the end.

Every example result on this page is for **Saturday, 14 November 2026, 10:20:30 UTC**.

Contents:

- [General rules](#general-rules)
- [Dates](#dates): [now and named days](#now-and-named-days), [times of day](#times-of-day),
  [calendar dates](#calendar-dates), [weekdays](#weekdays), [a date with a time](#a-date-with-a-time),
  [relative to now](#relative-to-now), [next, last and this](#next-last-and-this), [not dates](#not-dates)
- [Time zones](#time-zones), [daylight saving time](#daylight-saving-time)
- [Durations](#durations)
- [Differences from DateTime::Format::Natural](#differences-from-datetimeformatnatural)

## General rules

- Case does not matter: `Tomorrow`, `TOMORROW` and `tomorrow` are the same.
- Any amount of whitespace separates words.
- A comma at the end of a word is ignored, as is a word of only commas: `october 6, 2026` is `october 6 2026`.
- A comma inside a word is not: `october 6,2026` is not a date.
- Years have exactly four digits, `0001` to `9999`.
- Every date must lie in the years 1 to 9999, also the result of `in 3 days`.
- Dates that do not exist are rejected: `31 april`, `29 february 2027`, `2026-13-01`, `25:00`.
- A date is calculated on the calendar of its option's [time zone](#time-zones).
- An option also checks its bounds. Without a `min` or `max`, a date must lie between 1901-12-13T20:45:52Z and
  2038-01-19T03:14:07Z, and a duration may last at most 2147483647 seconds: what a cell holds. See
  [date](reference.md#date) and [duration](reference.md#duration).
- Quote values with spaces on the command line: `--at "tomorrow 3pm"`.

## Dates

### Now and named days

| Text | Means | Example result |
|---|---|---|
| `now` | the current time | 2026-11-14 10:20:30 |
| `today` | today, 00:00:00 | 2026-11-14 00:00:00 |
| `tomorrow` | tomorrow, 00:00:00 | 2026-11-15 00:00:00 |
| `yesterday` | yesterday, 00:00:00 | 2026-11-13 00:00:00 |
| `noon` | today, 12:00:00 | 2026-11-14 12:00:00 |
| `midnight` | today, 00:00:00 (not tomorrow) | 2026-11-14 00:00:00 |

`now` cannot be combined with anything.

### Times of day

A time alone means today at that time.

| Form | Examples |
|---|---|
| 24 hour clock, `H:MM` | `14:00`, `0:00`, `03:00`, `23:59` |
| 24 hour clock, `H:MM:SS` | `14:00:30`, `23:59:59` |
| 12 hour clock with am or pm | `3pm`, `3 pm`, `3am`, `12am`, `12pm` |
| with minutes and seconds | `3:30pm`, `3:30 pm`, `3:30:15pm` |
| `a.m.` and `p.m.` | `3a.m.`, `3 p.m.` |
| words | `noon`, `midnight` |

- Hours: 0 to 23 on the 24 hour clock, 1 to 12 with am or pm.
- `12am` is midnight, `12pm` is noon.
- Minutes and seconds always have two digits.
- Not times: `24:00`, `13pm`, `0pm`, `15:00 pm`, `3:5`, a bare hour such as `15` or `6`.

### Calendar dates

| Form | Examples | Example result |
|---|---|---|
| year-month-day | `2026-10-06`, `2026-1-6`, `2026/10/06`, `2026.10.06` | 2026-10-06 |
| ISO date and time, one word | `2026-10-06T14:00`, `2026-10-06T14:00:30` | 2026-10-06 14:00 |
| day.month.year | `06.10.2026`, `6.10.2026` | 2026-10-06 |
| day/month/year | `10/06/2026`, `31/12/2026` | 2026-06-10, 2026-12-31 |
| day-month-year | `06-10-2026` | 2026-10-06 |
| day month [year] | `6 october`, `6 oct 2026`, `6th october` | 2026-10-06 |
| month day [year] | `october 6`, `oct 6th`, `october 6, 2026` | 2026-10-06 |

Numbers with `/`, `.` or `-` are day, month, year, unless they start with a four digit year:

- `10/06/2026` is 10 June, not October 6.
- `12/31/2026` is not a date.

Month names, written out or in three letters: `january` `jan`, `february` `feb`, `march` `mar`, `april` `apr`, `may`,
`june` `jun`, `july` `jul`, `august` `aug`, `september` `sep`, `october` `oct`, `november` `nov`, `december` `dec`.
`sept` is not understood.

A day may have its English suffix, and it must be the right one: `1st`, `2nd`, `3rd`, `4th`, `11th`, `21st`, `22nd`,
`23rd`. `1th` and `11st` are rejected.

A day and month without a year is that day in the current year, even when it has passed: `6 october` is 2026-10-06.

### Weekdays

The week runs Monday to Sunday. A weekday is a day of this, next or last week, at 00:00:00.

| Text | Means | Example result |
|---|---|---|
| `monday` | Monday of this week, maybe in the past | 2026-11-09 |
| `this monday` | the same | 2026-11-09 |
| `sunday` | Sunday of this week | 2026-11-15 |
| `next monday` | Monday of next week | 2026-11-16 |
| `next saturday` | Saturday of next week, even when today is Saturday | 2026-11-21 |
| `last monday` | Monday of last week | 2026-11-02 |
| `last sunday` | Sunday of last week | 2026-11-08 |

Weekday names: `monday` `mon`, `tuesday` `tue` `tues`, `wednesday` `wed`, `thursday` `thu` `thur` `thurs`, `friday`
`fri`, `saturday` `sat`, `sunday` `sun`.

### A date with a time

A date may be followed by a time, optionally with `at` between. A date without a time is 00:00:00.

| Date | then a clock time | then `noon` or `midnight` |
|---|---|---|
| `today`, `tomorrow`, `yesterday` | yes | yes |
| numeric dates: `2026-10-06`, `06.10.2026`, `10/06/2026` | yes | yes |
| `6 october`, `october 6th` (no year) | yes | no |
| `6 october 2026` (with a year) | yes | no |
| weekdays: `monday`, `next monday` | yes | no |

Examples: `tomorrow 3pm`, `tomorrow at 15:00`, `yesterday at noon`, `today 9:30am`, `tomorrow noon`,
`2026-10-06 14:00:30`, `2026-10-06 3pm`, `06.10.2026 midnight`, `6 october 3pm`, `oct 6 at 3pm`,
`6 october 2026 14:00`, `monday 3pm`, `next monday at 3pm`, `this friday 9am`.

A clock time may also come first, without `at`, before `today`, `tomorrow`, `yesterday`, a day and month without a
year, or a weekday: `3pm tomorrow`, `15:00 tomorrow`, `3pm 6 october`, `3pm monday`, `3:30pm next thursday`.

Not understood:

- `noon` or `midnight` first: `noon tomorrow`, `midnight tomorrow`, `noon today`.
- `noon` after a weekday or a named month: `monday noon`, `6 october at noon`.
- A time first before a numeric date or a year: `3pm 2026-10-06`, `3pm 6 october 2026`.
- `at` first: `at 3pm tomorrow`, `at 3pm`.
- `now 3pm`, and two dates or two times in one text.

### Relative to now

A whole number and a unit, before `ago` or `from now`, or after `in`:

| Text | Example result |
|---|---|
| `3 days ago` | 2026-11-11 10:20:30 |
| `2 weeks ago` | 2026-10-31 10:20:30 |
| `1 year ago` | 2025-11-14 10:20:30 |
| `in 2 hours` | 2026-11-14 12:20:30 |
| `in 1 month` | 2026-12-14 10:20:30 |
| `5 minutes from now` | 2026-11-14 10:25:30 |
| `1 week from now` | 2026-11-21 10:20:30 |

- Units: `second`, `minute`, `hour`, `day`, `week`, `month`, `year`.
- Singular for 1 (`1 day ago`), plural otherwise (`2 days ago`). 0 takes either. `1 days ago` and `in 2 day` are
  rejected.
- No sign, no fraction, no words: `a day ago` and `in one hour` are rejected.
- No time after it: `in 3 days at 3pm` is rejected.

How the units count:

- Days, weeks, months and years keep the time of day on the calendar.
- Hours, minutes and seconds count elapsed time.
- So across a daylight saving change, `in 1 day` is the same time tomorrow, while `in 24 hours` is exactly 24 hours
  later.
- A month that lacks the day ends on its last day: on January 31, `in 1 month` is February 28 (or 29).

### Next, last and this

`next`, `last` or `this`, with a singular unit:

| Text | Means | Example result |
|---|---|---|
| `next year`, `last year`, `this year` | January 1 of that year, 00:00:00 | 2027-01-01, 2025-01-01, 2026-01-01 |
| `next month`, `last month`, `this month` | the 1st of that month, 00:00:00 | 2026-12-01, 2026-10-01, 2026-11-01 |
| `next week`, `last week` | 7 days after or before today, 00:00:00 | 2026-11-21, 2026-11-07 |
| `this week` | today, 00:00:00 | 2026-11-14 |
| `next day`, `last day`, `this day` | tomorrow, yesterday, today, 00:00:00 | 2026-11-15, 2026-11-13, 2026-11-14 |
| `next hour`, `last hour`, `this hour` | the start of that hour | 11:00:00, 09:00:00, 10:00:00 |
| `next minute`, `last minute`, `this minute` | the start of that minute | 10:21:00, 10:19:00, 10:20:00 |
| `next second`, `last second`, `this second` | one second after or before now, or now | 10:20:31, 10:20:29, 10:20:30 |

- `next week` is not the start of next week. For that, write `next monday`.
- Plural units (`next weeks`) and `next weekend` are rejected.

### Not dates

Besides the cases above, these are rejected:

- A month alone or with a year: `october`, `october 2026`.
- A bare number: `6`, `2026`.
- Two digit years: `6/10/26`, `26-10-06`.
- Times of day in words: `morning`, `afternoon`, `evening`, `tonight`.
- Spelled out numbers and articles: `a day ago`, `one week from now`, `the 6th of october`.
- `noon` or `midnight` before a date: `noon next monday`.
- Time zones and offsets in the text: `14:00 CET`, `2026-10-06T14:00:00Z`, `2026-10-06T14:00+02:00`.
- Abbreviated units: `3d ago`, `in 2h`.
- Combined units: `in 1 hour 30 minutes`.

## Time zones

An option reads its dates in one time zone, set with [`TimeZone`](reference.md#timezone). Default: `local`.

| Name | Means |
|---|---|
| `UTC` | Coordinated Universal Time. |
| `floating` | A date and time without a zone, calculated like UTC. |
| `local` | The server's own zone, see below. |
| IANA names | Any zone of the tz database: `Europe/Berlin`, `America/New_York`, `Asia/Kolkata`. |

IANA zones:

- Read from the directory in the `TZDIR` environment variable, else `/usr/share/zoneinfo`.
- A server without the tzdata package knows only `UTC`, `floating` and `local`, which then falls back as below.
- Names are case sensitive, like their file names.
- An empty or absolute name, or one with empty, `.` or `..` parts, is unknown. A name cannot point outside the
  zoneinfo directory.
- Zones with leap seconds (`right/...`) are not supported.
- After the last change stored in a zone file (such as dates in 2100), the file's final daylight saving rule applies.
  Right for zones whose rules do not change.

`local` is the first of these that works:

1. The `TZ` environment variable, when set. A leading `:` is ignored. Empty means UTC. Otherwise it must be a zone
   name as above. POSIX rule strings (`CET-1CEST,M3.5.0,M10.5.0/3`) and absolute paths are not read.
2. The file `/etc/localtime`.
3. The floating zone. The zone then calls itself `floating`, not `local`.

### Daylight saving time

- **Clocks forward**: an hour of the day does not exist. A date inside it moves forward by the gap. In
  `Europe/Berlin`, `2026-03-29 02:30` is 03:30 summer time.
- **Clocks back**: an hour of the day happens twice. A date inside it is the first one. In `Europe/Berlin`,
  `2026-10-25 02:30` is 02:30 summer time (00:30 UTC), not winter time.

## Durations

One whole number and one unit, optionally after `for`. Case and whitespace do not matter.

| Text | Length | Parts (months, days, seconds) |
|---|---|---|
| `90 seconds` | 90 seconds | 0, 0, 90 |
| `1 minute` | 60 seconds | 0, 0, 60 |
| `for 3 hours` | 10800 seconds | 0, 0, 10800 |
| `2 days` | 2 days | 0, 2, 0 |
| `2 weeks` | 14 days | 0, 14, 0 |
| `1 month` | 1 month | 1, 0, 0 |
| `for 1 year` | 12 months | 12, 0, 0 |

- Units: `second`, `minute`, `hour`, `day`, `week`, `month`, `year`.
- Singular for 1, plural otherwise, either for 0: `0 days`, `0 day`.
- Leading zeros are fine: `03 days`.
- A week is 7 days, a year is 12 months.
- Days and months have no fixed length in seconds. `1 day` keeps the time of day across a daylight saving change. One
  month after January 31 is the last day of February.
- A duration is added months first, then days, then seconds.

Longest: 10000 years in any unit. `10000 years`, `120000 months`, `521775 weeks`, `3652425 days`, `87658200 hours`,
`5259492000 minutes` or `315569520000 seconds`. Longer is rejected. An option's own `max` is usually far lower: see
[duration](reference.md#duration).

Not durations:

| Text | Why |
|---|---|
| `1h`, `30s`, `1 hr` | abbreviations |
| `1 hour 30 minutes` | two units |
| `1.5 hours` | a fraction |
| `-1 days` | a sign |
| `one day`, `a day` | words for numbers |
| `days` | no number |
| `1 days`, `2 day` | wrong singular or plural |
| `in 1 day`, `1 day ago` | dates |

As text, a duration is shown in its largest exact unit: 5400 seconds as `90 minutes`, 7200 seconds as `2 hours`,
14 days as `2 weeks`, 24 months as `2 years`.

## Differences from DateTime::Format::Natural

For the forms on this page, results equal those of DateTime::Format::Natural 1.27, except:

| Case | DateTime::Format::Natural | Here |
|---|---|---|
| Month ends, on January 31 | `in 1 month` is March 3, `next month` is March 1 | `in 1 month` is February 28, `next month` is February 1 |
| A time skipped by daylight saving (`2026-03-29 02:30`, `Europe/Berlin`) | rejected | moved forward by the gap |
| A time that happens twice | the second one (standard time) | the first one |
| `3 hours ago` just after clocks went back | can be only two hours ago | always three hours of elapsed time |
| Years outside 1 to 9999 (`3000 years ago`, `in 10000 years`) | accepted | rejected |

It also understands forms that are rejected here: `october`, `october 2026`, a bare number (`6` as 06:00, `2026` as
January 1), two digit years, `tomorrow evening` and other times of day in words, `noon` or `midnight` before `next`,
`last` or `this` with a weekday, and many more.
