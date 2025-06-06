SRCS=\
	script.c \
	physics.c \
	nuklear.c \
	engine/main.c \
	engine/model.c \
	engine/shader.c \
	engine/skeletal_mesh.c \
	engine/serialize/serialize.c \
	engine/serialize/skm_serialize.c \
	engine/stb_image.c \
	obj/shader.c \
	glad/src/glad.c 

TOOL_SHADER2C=\
	tool/shader2c.c

SHADERS=\
	shader/skel-frag.glsl \
	shader/skel-vert.glsl \
	shader/static-frag.glsl \
	shader/static-vert.glsl 

STATICLIBS=\
	-lmingw32 -lSDL3 -lSDL3_mixer -lopengl32 -lassimp -lz \
	-static -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm \
	-limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid -static-libgcc -lsetupapi \
	-lssp \
	-lshlwapi -mwindows 

CFLAGS=-Wall -g 

.PHONY: all web win clean

GAMENAME=bens-bales

ASSETS=\
	font-special-elite/SpecialElite.ttf \
	sounds/boing.wav \
	sounds/chomp.wav \
	blender/horse.glb \
	blender/hay.glb \
	blender/carrot.glb \
	sounds/music.ogg

all: win shader2c

# all:
# 	@echo "Please specify web or win"

win: bin/win/$(GAMENAME).exe
web: bin/web/index.html

shader2c: bin/shader2c.exe

bin/shader2c.exe: $(TOOL_SHADER2C:%.c=obj/tool/%.o) | bin/
	gcc $^ -o $@

obj/shader.c: $(SHADERS) bin/shader2c.exe | obj/
	bin/shader2c $(SHADERS) obj/shader.c obj/shader.h

obj/tool/%.o: %.c | $(addprefix obj/tool/,$(dir $(TOOL_SHADER2C)))
	gcc -MMD $(CFLAGS) -c $< -o $@ -I../SDL/include -I../assimp/include -I../assimp/build-win/include -Iglad/include -O2

bin/web/index.html: $(SRCS:%.c=obj/web/%.o) | bin/web/
	emcc $^ -o $@ -L../SDL/build-emcc/ -lSDL3_mixer -lSDL3 -lassimp -lzlibstatic -L../assimp/build-web/contrib/zlib -Wl,--gc-sections \
		-L../SDL_mixer/build-web \
		-L../assimp/build-web/lib \
		$(ASSETS:%=--embed-file %) \
		-sNO_DISABLE_EXCEPTION_CATCHING \
		-sALLOW_MEMORY_GROWTH \
		--shell-file=shell.html

bin/win/SDL3.dll: ../SDL/build-win/SDL3.dll
	cp ../SDL/build-win/SDL3.dll bin/win/

bin/win/SDL3_mixer.dll: ../SDL_mixer/build-win/SDL3_mixer.dll
	cp ../SDL_mixer/build-win/SDL3_mixer.dll bin/win/

bin/win/$(GAMENAME).exe: $(SRCS:%.c=obj/win/%.o) | bin/win/ bin/win/SDL3.dll bin/win/SDL3_mixer.dll
	g++ $^ -o $@ -L../SDL/build-win/ -L../SDL_mixer/build-win/ -L../assimp/build-win/lib -lSDL3_mixer -lSDL3 -lassimp -lz -Wl,--gc-sections

bin/dist/$(GAMENAME).exe: $(SRCS:%.c=obj/win/%.o) | bin/dist/
	mkdir -p bin/dist/blender
	mkdir -p bin/dist/sounds
	cp -r font-special-elite bin/dist
	cp sounds/boing.wav bin/dist/sounds
	cp sounds/chomp.wav bin/dist/sounds
	cp blender/horse.glb bin/dist/blender
	cp blender/hay.glb bin/dist/blender
	cp blender/carrot.glb bin/dist/blender
	cp sounds/music.ogg bin/dist/sounds
	g++ $^ -o $@ -L../SDL/build-win/ -L../SDL_mixer/build-win/ -L../assimp/build-win/lib $(STATICLIBS) -Wl,--gc-sections

obj/win/%.o: %.c | $(addprefix obj/win/,$(dir $(SRCS))) obj/shader.c
	gcc -MMD $(CFLAGS) -c $< -o $@ -I../SDL/include -I../SDL_mixer/include -I../assimp/include -I../assimp/build-win/include -Iglad/include -O2 -I. -ffunction-sections -DFAST_MODE

obj/web/%.o: %.c | $(addprefix obj/web/,$(dir $(SRCS))) obj/shader.c
	emcc $(CFLAGS) -c $< -o $@ -I../SDL/include -I../SDL_mixer/include -I../assimp/include -I../assimp/build-web/include -Iglad/include -I. -O2 -DFAST_MODE -I../cglm/include 

%/:
	mkdir -p $@

clean:
	rm -rf bin
	rm -rf obj

-include $(SRCS:%.c=obj/win/%.d)