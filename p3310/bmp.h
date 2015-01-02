/*
  Library to handle various phone functions.
  Includes a lightweight font and bitmap library and
  LCD drivers based on Adafruit_PCD8544
 
    Copyright (C) 2015 Cristiano Griletti

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/ 

//Map of bitmaps in memory
#define bmp330 0
#define bmp331 506
#define bmp332 1012
#define bmp333 1518
#define bmp334 2024
#define bmp335 2530
#define bmp007 3036
#define bmp015 3053
#define bmp079 3071
#define bmp080 3161
#define bmp085 3251
#define bmp086 3341
#define bmp087 3431
#define bmp088 3561
#define bmp089 3691
#define bmp090 3821
#define bmp091 3951
#define bmp092 4081
#define bmp093 4211
#define bmp094 4341
#define bmp095 4471
#define bmp096 4601
#define bmp097 4731
#define bmp098 4861
#define bmp099 4991
#define bmp100 5121
#define bmp101 5251
#define bmp102 5381
#define bmp103 5511
#define bmp104 5641
#define bmp105 5771
#define bmp106 5901
#define bmp107 6031
#define bmp108 6161
#define bmp109 6291
#define bmp110 6421
#define bmp111 6551
#define bmp112 6681
#define bmp113 6811
#define bmp114 6941
#define bmp115 7071
#define bmp116 7201
#define bmp117 7331
#define bmp118 7461
#define bmp119 7591
#define bmp120 7721
#define bmp121 7851
#define bmp122 7981
#define bmp123 8111
#define bmp124 8241
#define bmp125 8371
#define bmp126 8501
#define bmp127 8631
#define bmp128 8761
#define bmp129 8891
#define bmp130 9021
#define bmp131 9151
#define bmp132 9281
#define bmp133 9411
#define bmp134 9541
#define bmp135 9671
#define bmp136 9801
#define bmp137 9931
#define bmp138 10061
#define bmp139 10191
#define bmp140 10321
#define bmp141 10451
#define bmp142 10581
#define bmp143 10711
#define bmp144 10841
#define bmp145 10971
#define bmp146 11101
#define bmp147 11231
#define bmp148 11361
#define bmp149 11491
#define bmp150 11621
#define bmp151 11751
#define bmp152 11881
#define bmp153 12011
#define bmp154 12141
#define bmp155 12271
#define bmp156 12401
#define bmp157 12531
#define bmp158 12661
#define bmp159 12791
#define bmp160 12921
#define bmp161 13051
#define bmp162 13181
#define bmp163 13311
#define bmp164 13441
#define bmp165 13571
#define bmp166 13701
#define bmp167 13831
#define bmp168 13961
#define bmp169 14091
#define bmp170 14221
#define bmp171 14351
#define bmp172 14481
#define bmp173 14611
#define bmp174 14741
#define bmp175 14871
#define bmp176 15001
#define bmp177 15131
#define bmp178 15261
#define bmp179 15391
#define bmp180 15521
#define bmp181 15651
#define bmp182 15781
#define bmp183 15911
#define bmp184 16041
#define bmp185 16171
#define bmp186 16301
#define bmp187 16431
#define bmp188 16561
#define bmp189 16691
#define bmp190 16821
#define bmp191 16951
#define bmp192 17081
#define bmp193 17211
#define bmp194 17341
#define bmp195 17471
#define bmp196 17601
#define bmp197 17731
#define bmp198 17861
#define bmp199 17991
#define bmp200 18121
#define bmp201 18251
#define bmp202 18381
#define bmp203 18511
#define bmp204 18641
#define bmp205 18771
#define bmp206 18901
#define bmp207 19031
#define bmp208 19161
#define bmp209 19291
#define bmp210 19421
#define bmp211 19551
#define bmp212 19681
#define bmp213 19811
#define bmp214 19941
#define bmp215 20071
#define bmp216 20201
#define bmp226 20331
#define bmp227 20461
#define bmp229 20543
#define bmp230 20633
#define bmp231 20723
#define bmp232 20813
#define bmp233 20903
#define bmp234 20993
#define bmp235 21083
#define bmp236 21173
#define bmp237 21263
#define bmp238 21353
#define bmp239 21443
#define bmp240 21533
#define bmp241 21623
#define bmp242 21713
#define bmp243 21803
#define bmp244 21893
#define bmp245 21983
#define bmp246 22073
#define bmp247 22163
#define bmp248 22253
#define bmp249 22343
#define bmp250 22433
#define bmp251 22523
#define bmp252 22613
#define bmp253 22703
#define bmp254 22793
#define bmp255 22883
#define bmp256 22973
#define bmp257 23063
#define bmp258 23153
#define bmp259 23243
#define bmp260 23333
#define bmp261 23423
#define bmp262 23513
#define bmp263 23603
#define bmp264 23693
#define bmpogo 23783
#define bmp363 24289
#define bmp364 24795
#define bmp365 25301
#define bmp366 25807
#define bmp367 26313
