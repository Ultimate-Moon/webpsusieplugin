cc  = cl
link = link
# cflags：コンパイラオプション
cflags = /O2

TARGET= ifwebp
# dll：プラグインのファイル名
output = $(TARGET).spi
# objs：ソースの分だけ追加
objs = main.obj

MYLIB = libwebp.lib
MYDEF = spi00in.def
PACKSRC = $(MYLIB) $(MYDEF) *.cpp *.h makefile webp/*

# ---------------------------------------------------
# ソースファイルの拡張子が".c"の場合
.c.obj:
	$(cc) $(cflags) /c $<
# 使いまわせるように".cpp"も入れとく
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
