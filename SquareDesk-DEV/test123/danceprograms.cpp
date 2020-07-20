/****************************************************************************
**
** Copyright (C) 2016-2020 Mike Pogue, Dan Lyke
** Contact: mpogue @ zenstarstudio.com
**
** This file is part of the SquareDesk application.
**
** $SQUAREDESK_BEGIN_LICENSE$
**
** Commercial License Usage
** For commercial licensing terms and conditions, contact the authors via the
** email address above.
**
** GNU General Public License Usage
** This file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appear in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file.
**
** $SQUAREDESK_END_LICENSE$
**
****************************************************************************/

#include <stdio.h>
#include <danceprograms.h>

/* Callerlab as of 2018-09-01 */
const char *danceprogram_basic1[] = {
    " 1, Circle Left/Circle Right",
    " 2, Forward and Back",
    " 3, Dosado/Dosado to a Wave",
    " 4, Swing",
    " 5, Promenade Family",
    " 5.a, Couples (Full, 1/2, 3/4)",
    " 5.b, Single File Promenade",
    " 5.c, Wrong Way Promenade",
    " 5.d, Star Promenade",
    " 6, Allemande Left",
    " 7, Arm Turns",
    " 8, Right and Left Grand Family",
    " 8.a, Right and Left Grand",
    " 8.b, Weave the Ring",
    " 8.c, Wrong Way Grand",
    " 9, Left-Hand Star/Right-Hand Star",
    "10, Pass Thru",
    "11, Half Sashay Family",
    "11.a, Half Sashay",
    "11.b, Rollaway",
    "11.c, Ladies In, Men Sashay",
    "12, Turn Back Family",
    "12.a, U-Turn Back",
    "12.b, Backtrack",
    "13, Separate",
    "13.a, Around 1 or 2 to a Line",
    "13.b, Around 1 or 2 and Come Into the Middle",
    "14, Split Two",
    "15, Courtesy Turn",
    "16, Ladies Chain Family",
    "16.a, Two Ladies Chain (Reg. and 3/4)",
    "16.b, Four Ladies Chain (Reg. and 3/4)",
    "16.c, Chain Down the Line",
    "17, Do Paso",
    "18, Lead Right",
    "19, Veer Left/Veer Right",
    "20, Bend the Line",
    "21, Circulate Family",
    "21.a, (Named Dancers) Circulate",
    "21.b, Couples Circulate",
    "21.c, All Eight Circulate",
    "21.d, Single File Circulate",
    "21.e, Split/Box Circulate",
    "22, Right and Left Thru",
    "23, Grand Square",
    "24, Star Thru",
    "25, Double Pass Thru",
    "26, First Couple Go Left/Right, Next Couple Go Left/Right",
    "27, California Twirl",
    "28, Walk Around the Corner",
    "29, See Saw",
    "30, Square Thru (1, 2, 3, 4)/Left Square Thru (1, 2, 3, 4)",
    "31, Circle to a Line",
    "32, Dive Thru",
    nullptr
};

/* Callerlab as of 2018-09-01 */
const char *danceprogram_basic2[] = {
    "33, Wheel Around",
    "34, Box the Gnat",
    "35, Trade Family",
    "35.a, (Named Dancers) Trade",
    "35.b, Couples Trade",
    "35.c, Partner Trade",
    "36, Ocean Wave Family",
    "36.a, Step to a Wave",
    "36.b, Balance",
    "37, Alamo Style",
    "38, Swing Thru/Left Swing Thru",
    "39, Run/Cross Run",
    "40, Pass the Ocean",
    "41, Extend",
    "42, Wheel and Deal",
    "43, Zoom",
    "44, Flutterwheel/Reverse Flutterwheel",
    "45, Sweep a Quarter",
    "46, Trade By",
    "47, Touch 1/4",
    "48, Ferris Wheel",
    nullptr
};

/* Callerlab as of 2018-10-29 */
const char *danceprogram_mainstream[] = {
    "1, Cloverleaf",
    "2, Turn Thru",
    "3, Eight Chain Thru/Eight Chain 1, 2, 3, Etc.",
    "4, Pass to the Center",
    "5, Thar Family",
    "5.a, Allemande Thar",
    "5.b, Allemande Left to an Allemande Thar",
    "5.c, Wrong Way Thar",
    "6, Slip the Clutch",
    "7, Shoot the Star/Shoot the Star Full Around",
    " 8, Single Hinge/Couples Hinge",
    " 9, Centers In",
    "10, Cast Off 3/4",
    "11, Spin the Top",
    "12, Walk and Dodge",
    "13, Slide Thru",
    "14, Fold/Cross Fold",
    "15, Dixie Style to an Ocean Wave",
    "16, Spin Chain Thru",
    "17, Tag the Line Family",
    "17.a, Tag the Line (In/Out/Left/Right)",
    "17.b, Fraction (1/4, 1/2, 3/4) Tag",
    "18, Scoot Back",
    "19, Recycle (from a wave only)",
    nullptr
};

/* Callerlab as of 2018-09-22 */
const char *danceprogram_plus[] = {
    " 1, Acey Deucey",
    " 2, Teacup Chain",
    " 3, Ping Pong Circulate",
    " 4, Load the Boat",
    " 5, Peel Off",
    " 6, Linear Cycle (from waves only)",
    " 7, Coordinate",
    " 8, (Anything) and Spread",
    " 9, Spin Chain the Gears",
    "10, Track II",
    "11, (Anything) and Roll",
    "12, Follow Your Neighbor",
    "13, Fan the Top",
    "14, Explode the Wave",
    "15, Explode and (Anything) (from waves only)",
    "16, Relay the Deucey",
    "17, Peel the Top",
    "18, Diamond Circulate",
    "19, Single Circle to a Wave",
    "20, Trade the Wave",
    "21, Flip the Diamond",
    "22, Grand Swing Thru",
    "23, Crossfire",
    "24, All 8 Spin the Top",
    "25, Cut the Diamond",
    "26, Chase Right",
    "27, Dixie Grand",
    "28, Spin Chain and Exchange the Gears",
    nullptr
};

/* Callerlab as of 2018-09-15 */
const char *danceprogram_a1[] = {
    " 1, Belles and Beaus (to name dancers)",
    " 2, Brace Thru (formerly Half Breed Thru)",
    " 3, Cross Trail Thru",
    " 4, Triple Trade",
    " 5, Triple Scoot",
    " 6, Grand Follow Your Neighbor",
    " 7, Quarter Thru",
    " 8, Wheel Thru",
    " 9, Turn and Deal (2-faced lines, lines facing out)",
    "10, Pass In/Out",
    "11, Chain Reaction (1/4 tag)",
    "12, Mix",
    "13, Lockit",
    "14, Right (Left) Roll to a Wave",
    "15, Cast a Shadow",
    "16, Six-Two Acey-Deucey",
    "17, Clover and (Anything)",
    "18, Turn and Deal (ocean waves and other lines)",
    "19, Quarter In/Out",
    "20, Cross Over Circulate (two faced lines)",
    "21, Partner Tag",
    "22, Partner Hinge",
    "23, Horseshoe Turn",
    "24, Pass the Sea",
    "25, Split Square Thru",
    "26, Step and Slide",
    "27, Transfer the Column",
    "28, Cross Over Circulate (ocean waves and other lines)",
    "29, Swap Around",
    "30, Explode the Line",
    "31, As Couples Concept",
    "32, Ends Bend",
    "33, Square Chain Thru",
    "34, Scoot and Dodge",
    "35, Double Star Thru",
    "36, Left Wheel Thru",
    "37, (Anything) and Cross",
    "38, (Named Dancers) Cross",
    "39, Fractional Tops",
    "40, Three Quarter Thru",
    "41, Triple Star Thru",
    "42, Cycle and Wheel",
    "43, Grand Quarter Thru",
    "44, Grand Three Quarter Thru",
    "45, Explode and (Anything)",
    "46, Pair Off",
    "47, Reverse Swap Around",
    "48, Cross Clover and (Anything)",
    "49, Any Hand Concept",
    "50, Split Square Chain Thru",
    "51, Triple Cross/Double Cross",
    nullptr
};

/* Callerlab as of 2018-09-15 */
const char *danceprogram_a2[] = {
    " 1, Single Wheel",
    " 2, In Roll Circulate",
    " 3, Slip",
    " 4, Scoot and Weave",
    " 5, Split/Box Counter Rotate",
    " 6, Swing",
    " 7, Swing and Mix",
    " 8, Trade Circulate (from ocean waves)",
    " 9, Motivate",
    "10, Switch the Wave",
    "11, Pass and Roll",
    "12, Scoot Chain Thru",
    "13, Slide",
    "14, Recycle (facing couples)",
    "15, Spin the Windmill",
    "16, Out Roll Circulate",
    "17, Switch to a Diamond (from waves only)",
    "18, Hourglass Circulate",
    "19, Cut the Hourglass",
    "20, Flip the Hourglass",
    "21, Pass and Roll Your Neighbor",
    "22, Trade Circulate (from two faced lines)",
    "23, Zig Zag/Zag Zig",
    "24, Checkmate the Column",
    "25, Mini-Busy",
    "26, Slither",
    "27, Trail Off",
    "28, Remake Family",
    "28.a, Remake",
    "28.b, Grand Remake",
    "28.c, Remake The Thar",
    "29, Switch to an Hourglass (from parallel waves only)",
    "30, Split/Box Transfer",
    "31, Diamond Chain Thru",
    "32, Peel and Trail (from completed double pass thru)",
    "33, Peel and Trail (from columns)",
    "34, Transfer and (Anything)",
    "35, All 4 Couples/All 8 Concept, such as:",
    "35.b, All 4 Couples Star Thru",
    "35.a, All 4 Couples Right and Left Thru",
    "35.c, All 4 Couples Chase Right",
    "35.d, All 8 Swing Thru",
    "35.e, All 8 Switch the Wave",
    "35.f, All 8 Walk and Dodge",
    "35.g, All 8 Mix",
    nullptr
};

static const char *b1 = "b1";
static const char *b2 = "b2";
static const char *ms = "ms";
static const char *plus = "plus";
static const char *a1 = "a1";
static const char *a2 = "a2";
const struct DanceProgramCallInfo danceprogram_callinfo[] = {
    { b1, "Circle Left/Circle Right", "1/2: 4, 3/4: 6, Full: 8" },
    { b1, "Forward and Back", "Lines close together: 4; All others: 8" },
    { b1, "Dosado/Dosado to a Wave", "SS with corner, 6 steps; with partner, 6; from a Box formation, 6; SS across the set, 8" },
    { b1, "Swing", "Variable, at least 4" },
    { b1, "Promenade Family", NULL },
    { b1, "Couples (Full, 1/2, 3/4)", "1/4: 4, 1/2: 8, 3/4: 12, Full: 16" },
    { b1, "Single File Promenade", NULL },
    { b1, "Wrong Way Promenade", NULL },
    { b1, "Star Promenade", "1/2: 6, 3/4: 9, Full: 12, Full plus a back out at home: 16" },
    { b1, "Allemande Left", "1/2 arm turn: 4-6; 3/4 arm turn: 6-8; Full arm turn: 8" },
    { b1, "Arm Turns", "1/2: 4, 3/4: 4 to 6, Full: 6 to 8" },
    { b1, "Right and Left Grand Family", "10" },
    { b1, "Right and Left Grand", "10" },
    { b1, "Weave the Ring", "10" },
    { b1, "Wrong Way Grand", "10" },
    { b1, "Left-Hand Star/Right-Hand Star", "1/2: 4, 3/4: 6, Full: 8" },
    { b1, "Pass Thru", "2" },
    { b1, "Half Sashay Family", "4" },
    { b1, "Half Sashay", "4" },
    { b1, "Rollaway", "4" },
    { b1, "Ladies In, Men Sashay", "4" },
    { b1, "Turn Back Family", "2" },
    { b1, "U-Turn Back", "2" },
    { b1, "Backtrack", "2" },
    { b1, "Separate", "2, or determined by the distance traveled around the outside" },
    { b1, "Around 1 or 2 to a Line", "Heads Pass Thru; Separate Around 1 To A Line: 8, Heads Pass Thru; Separate Around 2 To A Line: 10" },
    { b1, "Around 1 or 2 and Come Into the Middle", NULL },
    { b1, "Split Two", "2" },
    { b1, "Courtesy Turn", "4" },
    { b1, "Ladies Chain Family", "Facing Couples 6; Squared Set: 8" },
    { b1, "Two Ladies Chain (Reg. and 3/4)", "Facing Couples: 6; Squared Set: 8" },
    { b1, "Four Ladies Chain (Reg. and 3/4)", "Regular 8; 3/4: 10" },
    { b1, "Chain Down the Line", "8" },
    { b1, "Do Paso", "12" },
    { b1, "Lead Right", "4" },
    { b1, "Veer Left/Veer Right", "2" },
    { b1, "Veer Left", "2" },
    { b1, "Veer Right", "2" },
    { b1, "Veer", "2" },
    { b1, "Bend the Line", "4" },
    { b1, "Circulate Family", "Single File Circulate, 2; all other circulates, 4" },
    { b1, "(Named Dancers) Circulate", "4" },
    { b1, "Circulate", "4" },
    { b1, "Couples Circulate", "4" },
    { b1, "All Eight Circulate", "4" },
    { b1, "Single File Circulate", "2" },
    { b1, "Split/Box Circulate", "4" },
    { b1, "Split Circulate", "4" },
    { b1, "Box Circulate", "4" },
    { b1, "Right and Left Thru", "SS, 8: Box or Ocean Wave, 6" },
    { b1, "Grand Square", "32" },
    { b1, "Star Thru", "4" },
    { b1, "Double Pass Thru", "4 steps." },
    { b1, "First Couple Go Left/Right, Next Couple Go Left/Right", "6" },
    { b1, "California Twirl", "4" },
    { b1, "Walk Around the Corner", "8" },
    { b1, "See Saw", "8" },
    { b1, "Square Thru (1, 2, 3, 4)/Left Square Thru (1, 2, 3, 4)", "1: 2, 2: 5, 3: 7 or 8, 4: 10" },
    { b1, "Square Thru 1", "2" },
    { b1, "Square Thru 2", "5" },
    { b1, "Square Thru 3", "7 or 8" },
    { b1, "Square Thru 4", "10" },
    { b1, "Circle to a Line", "8" },
    { b1, "Dive Thru", "Couple diving under: 2, couple making the arch: 6" },
    { b2, "Wheel Around", "4" },
    { b2, "Box the Gnat", "4" },
    { b2, "Trade Family", NULL },
    { b2, "(Named Dancers) Trade", "3, with additional beats for non-adjacent dancers" },
    { b2, "Couples Trade", "4" },
    { b2, "Partner Trade", "3" },
    { b2, "Ocean Wave Family", NULL },
    { b2, "Step to a Wave", "2" },
    { b2, "Balance", "4" },
    { b2, "Alamo Style", "4" },
    { b2, "Swing Thru/Left Swing Thru", "6" },
    { b2, "Swing Thru", "6" },
    { b2, "Left Swing Thru", "6" },
    { b2, "Run/Cross Run", "4; 6" },
    { b2, "Run", "4" },
    { b2, "Cross Run", "4" },
    { b2, "Pass the Ocean", "4" },
    { b2, "Extend", "2" },
    { b2, "Wheel and Deal", "4" },
    { b2, "Zoom", "4" },
    { b2, "Flutterwheel/Reverse Flutterwheel", "8 (SS All four ladies, 12 steps)" },
    { b2, "Flutterwheel", "8 (SS All four ladies, 12 steps)" },
    { b2, "Reverse Flutterwheel", "8 (SS All four ladies, 12 steps)" },
    { b2, "Sweep a Quarter", "2 couples, 2 steps; all 4 couples, 4 steps." },
    { b2, "Trade By", "4 steps." },
    { b2, "Touch 1/4", "2" },
    { b2, "Touch a Quarter", "2" },
    { b2, "Ferris Wheel", "6" },
    { ms, "Cloverleaf", "8" },
    { ms, "Turn Thru", "4" },
    { ms, "Eight Chain Thru/Eight Chain 1, 2, 3, Etc.", "20; each odd-numbered part: 2; each even-numbered part: 3" },
    { ms, "Pass to the Center", "2 & 6" },
    { ms, "Thar Family", NULL },
    { ms, "Allemande Thar", "2 (for the Left Arm Turn 1/2)" },
    { ms, "Allemande Left to an Allemande Thar", "12" },
    { ms, "Wrong Way Thar", "2 (for the Right Arm Turn 1/2)" },
    { ms, "Slip the Clutch", "2" },
    { ms, "Shoot the Star/Shoot the Star Full Around", "4; full around: 6" },
    { ms, "Single Hinge/Couples Hinge", "2 steps, 3 steps" },
    { ms, "Shoot the Star", "4" },
    { ms, "Centers In", "2" },
    { ms, "Shoot the Star Full Around", "6" },
    { ms, "Cast Off 3/4", "6" },
    { ms, "Spin the Top", "8" },
    { ms, "Single Hinge", "2" },
    { ms, "Walk and Dodge", "4" },
    { ms, "Hinge", "2" },
    { ms, "Slide Thru", "4" },
    { ms, "Couples Hinge", "3" },
    { ms, "Fold/Cross Fold", "2; 4" },
    { ms, "Dixie Style to an Ocean Wave", "6; all 4 couples 8" },
    { ms, "Spin Chain Thru", "16 steps" },
    { ms, "Cast Off Three Quarters", "6" },
    { ms, "Tag the Line Family", NULL },
    { ms, "Tag the Line (In/Out/Left/Right)", "6" },
    { ms, "Fraction (1/4, 1/2, 3/4) Tag", "3, 4, 5" },
    { ms, "Scoot Back", "6 steps" },
    { ms, "Recycle (from a wave only)", "4" },
    { ms, "Fold", "2" },
    { ms, "Cross Fold", "4" },
    { plus, "Acey Deucey", "4" },
    { plus, "Teacup Chain", "32" },
    { plus, "Ping Pong Circulate", "6" },
    { plus, "Load the Boat", "12" },
    { plus, "Peel Off", "4" },
    { plus, "Linear Cycle (from waves only)", "8-10" },
    { plus, "Coordinate", "8" },
    { plus, "(Anything) and Spread", "2" },
    { plus, "Spread", "2" },
    { plus, "Spin Chain the Gears", "24" },
    { plus, "Track II", "8" },
    { plus, "(Anything) and Roll", "2" },
    { plus, "Follow Your Neighbor", "6" },
    { plus, "Fan the Top", "4" },
    { plus, "Explode the Wave", "6" },
    { plus, "Explode and (Anything) (from waves only)", "2" },
    { plus, "Relay the Deucey", "20" },
    { plus, "Peel the Top", "6" },
    { plus, "Diamond Circulate", "3" },
    { plus, "Single Circle to a Wave", "4" },
    { plus, "Trade the Wave", "6" },
    { plus, "Flip the Diamond", "3" },
    { plus, "Grand Swing Thru", "6" },
    { plus, "Crossfire", "6" },
    { plus, "All 8 Spin the Top", "10" },
    { plus, "Cut the Diamond", "6" },
    { plus, "Chase Right", "6" },
    { plus, "Dixie Grand", "6" },
    { plus, "Spin Chain and Exchange the Gears", "26" },
    { a1, "Belles and Beaus (to name dancers)", NULL },
    { a1, "Brace Thru (formerly Half Breed Thru)", "6, SS 8" },
    { a1, "Cross Trail Thru", "6, SS 6" },
    { a1, "Triple Trade", "4" },
    { a1, "Triple Scoot", "6" },
    { a1, "Grand Follow Your Neighbor", "6" },
    { a1, "Quarter Thru", "6" },
    { a1, "Wheel Thru", "4, SS 6" },
    { a1, "Turn and Deal (2-faced lines, lines facing out)", "4" },
    { a1, "Pass In/Out", "4" },
    { a1, "Chain Reaction (1/4 tag)", "12" },
    { a1, "Mix", "6" },
    { a1, "Lockit", "4" },
    { a1, "Right (Left) Roll to a Wave", "4 & 2" },
    { a1, "Cast a Shadow", "10" },
    { a1, "Six-Two Acey-Deucey", "4" },
    { a1, "Clover and (Anything)", "Greater of 4 or the call" },
    { a1, "Turn and Deal (ocean waves and other lines)", "4" },
    { a1, "Quarter In/Out", "2" },
    { a1, "Cross Over Circulate (two faced lines)", "6" },
    { a1, "Partner Tag", "3" },
    { a1, "Partner Hinge", "2" },
    { a1, "Horseshoe Turn", "6" },
    { a1, "Pass the Sea", "6" },
    { a1, "Split Square Thru", "6-8-10, SS 8-10-12" },
    { a1, "Step and Slide", "4" },
    { a1, "Transfer the Column", "10" },
    { a1, "Cross Over Circulate (ocean waves and other lines)", "6" },
    { a1, "Swap Around", "4" },
    { a1, "Explode the Line", "6" },
    { a1, "As Couples Concept", NULL },
    { a1, "Ends Bend", "2" },
    { a1, "Square Chain Thru", "14" },
    { a1, "Scoot and Dodge", "8" },
    { a1, "Double Star Thru", "6" },
    { a1, "Left Wheel Thru", "4, SS 6" },
    { a1, "(Anything) and Cross", "call + 2" },
    { a1, "(Named Dancers) Cross", "2" },
    { a1, "Fractional Tops", "4-6-8" },
    { a1, "Three Quarter Thru", "8" },
    { a1, "Triple Star Thru", "10" },
    { a1, "Cycle and Wheel", "4" },
    { a1, "Grand Quarter Thru", "6" },
    { a1, "Grand Three Quarter Thru", "8" },
    { a1, "Explode and (Anything)", "2 + call" },
    { a1, "Pair Off", "2, SS 4" },
    { a1, "Reverse Swap Around", "4" },
    { a1, "Cross Clover and (Anything)", "greater of 10 or the call" },
    { a1, "Any Hand Concept", NULL },
    { a1, "Split Square Chain Thru", "12" },
    { a1, "Triple Cross/Double Cross", "4" },
    { a2, "Single Wheel", "4" },
    { a2, "In Roll Circulate", "4" },
    { a2, "Slip", "3" },
    { a2, "Scoot and Weave", "10" },
    { a2, "Split/Box Counter Rotate", "4" },
    { a2, "Box Counter Rotate", "4" },
    { a2, "Split Counter Rotate", "4" },
    { a2, "Swing", "3" },
    { a2, "Swing and Mix", "8" },
    { a2, "Trade Circulate (from ocean waves)", "6" },
    { a2, "Motivate", "16" },
    { a2, "Switch the Wave", "6" },
    { a2, "Pass and Roll", "10" },
    { a2, "Scoot Chain Thru", "12" },
    { a2, "Slide", "3" },
    { a2, "Recycle (facing couples)", "6" },
    { a2, "Spin the Windmill", "12" },
    { a2, "Out Roll Circulate", "6" },
    { a2, "Switch to a Diamond (from waves only)", "4" },
    { a2, "Hourglass Circulate", "4" },
    { a2, "Cut the Hourglass", "6" },
    { a2, "Flip the Hourglass", "4" },
    { a2, "Pass and Roll Your Neighbor", "12" },
    { a2, "Trade Circulate (from two faced lines)", "6" },
    { a2, "Zig Zag/Zag Zig", "2" },
    { a2, "Zig-Zag", "2" },
    { a2, "Zag-Zig", "2" },
    { a2, "Zig Zag", "2" },
    { a2, "Zag Zig", "2" },
    { a2, "Zig", "2" },
    { a2, "Zab", "2" },
    { a2, "Checkmate the Column", "10" },
    { a2, "Mini-Busy", "6" },
    { a2, "Slither", "3" },
    { a2, "Trail Off", "6" },
    { a2, "Remake Family", NULL },
    { a2, "Remake", "Alamo 12, others 10" },
    { a2, "Grand Remake", "10" },
    { a2, "Remake The Thar", "10" },
    { a2, "Switch to an Hourglass (from parallel waves only)", "4" },
    { a2, "Split/Box Transfer", "8" },
    { a2, "Split Transfer", "8" },
    { a2, "Box Transfer", "8" },
    { a2, "Diamond Chain Thru", "10" },
    { a2, "Peel and Trail (from completed double pass thru)", "4" },
    { a2, "Peel and Trail (from columns)", "6" },
    { a2, "Transfer and (Anything)", "8 (any starts on 5)" },
    { a2, "All 4 Couples/All 8 Concept, such as:", NULL },
    { a2, "All 4 Couples Star Thru", "6" },
    { a2, "All 4 Couples Right and Left Thru", "10" },
    { a2, "All 4 Couples Chase Right", "10" },
    { a2, "All 8 Swing Thru", "8" },
    { a2, "All 8 Switch the Wave", nullptr }, // NULL },
    { a2, "All 8 Walk and Dodge", "6" },
    { a2, "All 8 Mix", nullptr }, // NULL },
    { nullptr, nullptr, nullptr }
//    { NULL, NULL, NULL }
};

