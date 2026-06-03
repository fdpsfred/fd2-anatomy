echo === compile src === > E:\out\build.out
D:\BIN\WCC386.EXE table\table.c %CF% -fo=E:\out\table.obj >> E:\out\build.out
D:\BIN\WCC386.EXE battle\battle.c %CF% -fo=E:\out\battle.obj >> E:\out\build.out
D:\BIN\WCC386.EXE spell\spell.c %CF% -fo=E:\out\spell.obj >> E:\out\build.out
D:\BIN\WCC386.EXE spell\spelleff.c %CF% -fo=E:\out\spelleff.obj >> E:\out\build.out
D:\BIN\WCC386.EXE input\input.c %CF% -fo=E:\out\input.obj >> E:\out\build.out
D:\BIN\WCC386.EXE gfx\palette.c %CF% -fo=E:\out\palette.obj >> E:\out\build.out
D:\BIN\WCC386.EXE ui_menu\cursor.c %CF% -fo=E:\out\cursor.obj >> E:\out\build.out
D:\BIN\WCC386.EXE ui_menu\status.c %CF% -fo=E:\out\status.obj >> E:\out\build.out
D:\BIN\WCC386.EXE util\misc.c %CF% -fo=E:\out\misc.obj >> E:\out\build.out
D:\BIN\WCC386.EXE util\dpmi.c %CF% -fo=E:\out\dpmi.obj >> E:\out\build.out
D:\BIN\WCC386.EXE anim\aniwalk.c %CF% -fo=E:\out\aniwalk.obj >> E:\out\build.out
D:\BIN\WCC386.EXE anim\anisummn.c %CF% -fo=E:\out\anisummn.obj >> E:\out\build.out
D:\BIN\WCC386.EXE anim\aniui.c %CF% -fo=E:\out\aniui.obj >> E:\out\build.out
D:\BIN\WCC386.EXE anim\anidec.c %CF% -fo=E:\out\anidec.obj >> E:\out\build.out
D:\BIN\WCC386.EXE util\pathfnd.c %CF% -fo=E:\out\pathfnd.obj >> E:\out\build.out
D:\BIN\WCC386.EXE battle\btl_turn.c %CF% -fo=E:\out\btlturn.obj >> E:\out\build.out
D:\BIN\WCC386.EXE battle\btl_init.c %CF% -fo=E:\out\btlinit.obj >> E:\out\build.out
D:\BIN\WCC386.EXE battle\btl_ai.c %CF% -fo=E:\out\btlai.obj >> E:\out\build.out
D:\BIN\WCC386.EXE battle\btl_aitg.c %CF% -fo=E:\out\btlaitg.obj >> E:\out\build.out
D:\BIN\WCC386.EXE battle\btl_aisc.c %CF% -fo=E:\out\btlaisc.obj >> E:\out\build.out
D:\BIN\WCC386.EXE audio\audio.c %CF% -fo=E:\out\audio.obj >> E:\out\build.out
D:\BIN\WCC386.EXE life\main.c %CF% -fo=E:\out\lifemain.obj >> E:\out\build.out
D:\BIN\WCC386.EXE save\save.c %CF% -fo=E:\out\save.obj >> E:\out\build.out
D:\BIN\WCC386.EXE util\noop.c %CF% -fo=E:\out\noop.obj >> E:\out\build.out
D:\BIN\WCC386.EXE rsrc\rsrc.c %CF% -fo=E:\out\rsrc.obj >> E:\out\build.out
D:\BIN\WCC386.EXE ui_menu\menu.c %CF% -fo=E:\out\menu.obj >> E:\out\build.out
D:\BIN\WCC386.EXE gfx\rndscene.c %CF% -fo=E:\out\rndscene.obj >> E:\out\build.out
D:\BIN\WCC386.EXE gfx\blitspr.c %CF% -fo=E:\out\blitspr.obj >> E:\out\build.out

echo === compile tests === >> E:\out\build.out
D:\BIN\WCC386.EXE E:\testmain.c %CF% -fo=E:\out\testmain.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\testglob.c %CF% -fo=E:\out\testglob.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\anim\anidec.c %CF% -fo=E:\out\tanidec.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\anim\anisumm1.c %CF% -fo=E:\out\tanisum1.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\anim\anisumm2.c %CF% -fo=E:\out\tanisum2.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\anim\aniui.c %CF% -fo=E:\out\taniui.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\anim\aniwalk1.c %CF% -fo=E:\out\taniwal1.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\anim\aniwalk2.c %CF% -fo=E:\out\taniwal2.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\audio\audio.c %CF% -fo=E:\out\taudio.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\battle\battle1.c %CF% -fo=E:\out\tbattle1.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\battle\battle2.c %CF% -fo=E:\out\tbattle2.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\battle\btl_ai.c %CF% -fo=E:\out\tbtlai.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\battle\btl_ais1.c %CF% -fo=E:\out\tbtlais1.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\battle\btl_ais2.c %CF% -fo=E:\out\tbtlais2.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\battle\btl_aitg.c %CF% -fo=E:\out\tbtlaitg.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\battle\btl_init.c %CF% -fo=E:\out\tbtlinit.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\battle\btl_turn.c %CF% -fo=E:\out\tbtlturn.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\gfx\blitspr.c %CF% -fo=E:\out\tblitspr.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\gfx\palette.c %CF% -fo=E:\out\tpalette.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\gfx\rndscene.c %CF% -fo=E:\out\trndscen.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\input\input.c %CF% -fo=E:\out\tinput.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\life\main.c %CF% -fo=E:\out\tmain.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\rsrc\rsrc.c %CF% -fo=E:\out\trsrc.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\save\save.c %CF% -fo=E:\out\tsave.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\spell\spell.c %CF% -fo=E:\out\tspell.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\spell\spelleff.c %CF% -fo=E:\out\tspellef.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\table\table.c %CF% -fo=E:\out\ttable.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\ui_menu\cursor.c %CF% -fo=E:\out\tcursor.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\ui_menu\menu.c %CF% -fo=E:\out\tmenu.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\ui_menu\status.c %CF% -fo=E:\out\tstatus.obj >> E:\out\build.out
D:\BIN\WCC386.EXE E:\util\pathfnd.c %CF% -fo=E:\out\tpathfnd.obj >> E:\out\build.out

echo === link === >> E:\out\build.out
D:\BIN\WLINK.EXE @E:\test.lnk >> E:\out\build.out

echo === run === >> E:\out\build.out
E:\out\TEST.EXE > E:\out\test.out

echo done > E:\out\done.txt
exit
