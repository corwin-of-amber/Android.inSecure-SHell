NDK = /opt/android/sdk/ndk/20.0.5594570
NDK_LLVM_BIN = $(NDK)/toolchains/llvm/prebuilt/darwin-x86_64/bin

PATH := $(NDK_LLVM_BIN):$(PATH)
export PATH

LDFLAGS = -EL -z now -z relro -X --hash-style=both --enable-new-dtags \
		  --eh-frame-hdr -m armelf_linux_eabi -dynamic-linker /system/bin/linker

# Cross build (NDK)
AS = $(NDK_LLVM_BIN)/arm-linux-androideabi-as
CC = $(NDK_LLVM_BIN)/clang
CFLAGS = -target arm-linux-androideabi -D__ANDROID_API__=20

LD = $(NDK_LLVM_BIN)/arm-linux-androideabi-ld

ISHD_OBJ = obj/infra/sys/start.o \
		   ${addprefix obj/server/fork-select/, main.o control.o} \
	       ${addprefix obj/infra/glibc/, login_tty.o openpty.o forkpty.o}

ISH_OBJ = obj/infra/sys/start.o \
		  ${addprefix obj/client/select/, main.o control.o}

# Host build (gcc)
HOST_ISHD_OBJ = ${addprefix obj/host/server/fork-select/, main.o control.o}
HOST_ISH_OBJ  = ${addprefix obj/host/client/select/, main.o control.o}

HOSTCC = gcc
HOSTLD = $(HOSTCC)

default: bin/ishd bin/oish

bin/ishd: $(ISHD_OBJ) Makefile
	$(LD) $(LDFLAGS) -o $@ $(ISHD_OBJ) -Linfra/sys -lc

bin/ish: $(ISH_OBJ) Makefile
	$(LD) $(LDFLAGS) -o $@ $(ISH_OBJ) -Linfra/sys -lc

bin/oishd: $(HOST_ISHD_OBJ) Makefile
	$(HOSTLD) $(HOST_ISHD_OBJ) -o $@

bin/oish: $(HOST_ISH_OBJ) Makefile
	$(HOSTLD) $(HOST_ISH_OBJ) -o $@

all: bin/ishd bin/ish bin/oishd bin/oish

obj/%.o: %.s
	@mkdir -p ${dir $@}
	$(AS) -c $< -o $@

obj/%.o: %.c
	@mkdir -p ${dir $@}
	$(CC) $(CFLAGS) -c $< -o $@

obj/host/%.o: %.c
	@mkdir -p ${dir $@}
	$(HOSTCC) -c $< -o $@

run: run-ishd

run-%:
	adb push bin/$* /data/local/tmp/
	adb shell /data/local/tmp/$* $(A)

%-run: bin/%
	adb push bin/$* /data/local/tmp/
	adb shell /data/local/tmp/$* $(A)
