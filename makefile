!include "../global.mak"

ALL : "$(OUTDIR)\MQ2Medley.dll"

CLEAN :
	-@erase "$(INTDIR)\MQ2Medley.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\MQ2Medley.dll"
	-@erase "$(OUTDIR)\MQ2Medley.exp"
	-@erase "$(OUTDIR)\MQ2Medley.lib"
	-@erase "$(OUTDIR)\MQ2Medley.pdb"


LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib $(DETLIB) ..\Release\MQ2Main.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\MQ2Medley.pdb" /debug /machine:I386 /out:"$(OUTDIR)\MQ2Medley.dll" /implib:"$(OUTDIR)\MQ2Medley.lib" /OPT:NOICF /OPT:NOREF 
LINK32_OBJS= \
	"$(INTDIR)\MQ2Medley.obj" \
	"$(OUTDIR)\MQ2Main.lib"

"$(OUTDIR)\MQ2Medley.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) $(LINK32_FLAGS) $(LINK32_OBJS)


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("MQ2Medley.dep")
!INCLUDE "MQ2Medley.dep"
!ELSE 
!MESSAGE Warning: cannot find "MQ2Medley.dep"
!ENDIF 
!ENDIF 


SOURCE=.\MQ2Medley.cpp

"$(INTDIR)\MQ2Medley.obj" : $(SOURCE) "$(INTDIR)"

