cc  = cl
link = link
# cflags�F�R���p�C���I�v�V����
cflags = /O2

TARGET= ifwebp
# dll�F�v���O�C���̃t�@�C����
output = $(TARGET).spi
# objs�F�\�[�X�̕������ǉ�
objs = main.obj

MYLIB = libwebp.lib
MYDEF = spi00in.def
PACKSRC = $(MYLIB) $(MYDEF) *.cpp *.h makefile webp/*

# ---------------------------------------------------
# �\�[�X�t�@�C���̊g���q��".c"�̏ꍇ
.c.obj:
	$(cc) $(cflags) /c $<
# �g���܂킹��悤��".cpp"������Ƃ�
.cpp.obj:
	$(cc) $(cflags) /c $<
all: $(output)
	#copy $(output) C:\Programs\afx\Susie
comp: clean all

$(output): $(objs)
	$(link) $(objs) advapi32.lib kernel32.lib $(MYLIB) /DEF:$(MYDEF) /DLL /nologo /MACHINE:X86 /OUT:$(output)
#	$(link) $(objs) advapi32.lib kernel32.lib $(MYLIB) /nologo /MACHINE:X86 /OUT:$(output)

pack: $(output)
	@if not exist $(TARGET) mkdir $(TARGET)
	zip -1 $(TARGET)\src.zip $(PACKSRC) 
	copy $(output) $(TARGET)\\
	copy readme.txt $(TARGET)\\
	zip -r1 $(TARGET).zip $(TARGET)

clean:
	del $(output) $(objs)
