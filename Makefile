# Copyright (c) 2024 Roger Brown.
# Licensed under the MIT License.

SRC=less.c
CL=cl
OBJDIR=obj\$(VSCMD_ARG_TGT_ARCH)
BINDIR=bin\$(VSCMD_ARG_TGT_ARCH)
RESFILE=$(OBJDIR)\less.res
APP=$(BINDIR)\less.exe
MSI=less-$(DEPVERS_less_STR4)-$(VSCMD_ARG_TGT_ARCH).msi

all: $(APP) $(MSI) $(MSIX)
	
clean: 
	if exist $(APP) del $(APP)
	if exist $(OBJDIR)\*.obj del $(OBJDIR)\*.obj
	if exist $(OBJDIR) rmdir $(OBJDIR)
	if exist $(BINDIR) rmdir $(BINDIR)

$(APP): $(SRC) $(OBJDIR) $(BINDIR) $(RESFILE)
	$(CL) 							\
		/Fe$@ 						\
		/Fo$(OBJDIR)\				\
		/W3 						\
		/WX 						\
		/MT 						\
		/I.							\
		/DNDEBUG 					\
		/D_CRT_SECURE_NO_DEPRECATE 	\
		/D_CRT_NONSTDC_NO_DEPRECATE \
		/DHAVE_LIMITS_H				\
		/DHAVE_FCNTL_H				\
		/DWIN32_LEAN_AND_MEAN		\
		$(SRC) 						\
		/link						\
		/INCREMENTAL:NO				\
		/PDB:NONE					\
		/SUBSYSTEM:CONSOLE			\
		user32.lib					\
		/DEF:less.def				\
		$(RESFILE)
	del "$(BINDIR)\less.exp"
	del "$(BINDIR)\less.lib"
	signtool sign /sha1 "$(CertificateThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@

$(RESFILE): less.rc
	rc /r $(RCFLAGS) "/DDEPVERS_less_INT4=$(DEPVERS_less_INT4)" "/DDEPVERS_less_STR4=\"$(DEPVERS_less_STR4)\"" /fo$@ less.rc

$(OBJDIR) $(BINDIR):
	mkdir $@

$(MSI): $(APP)
	"$(WIX)bin\candle.exe" -nologo "wix\$(VSCMD_ARG_TGT_ARCH).wxs" -dDEPVERS_less_STR4=$(DEPVERS_less_STR4)
	"$(WIX)bin\light.exe" -nologo -cultures:null -out $@ "$(VSCMD_ARG_TGT_ARCH).wixobj"
	del "$(VSCMD_ARG_TGT_ARCH).wixobj"
	del "less-$(DEPVERS_less_STR4)-$(VSCMD_ARG_TGT_ARCH).wixpdb"
	signtool sign /sha1 "$(CertificateThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@
