SRCS=\
	script.c \
	engine/main.c \
	engine/model.c \
	engine/shader.c \
	engine/skeletal_mesh.c \
	engine/serialize/serialize.c \
	engine/serialize/skm_serialize.c \
	obj/shader.c \
	glad/src/glad.c 

TOOL_SHADER2C=\
	tool/shader2c.c

SHADERS=\
	shader/skel-frag.glsl \
	shader/skel-vert.glsl

CFLAGS=-Wall -g 

.PHONY: all web win clean

GAMENAME=game

all: win shader2c

# all:
# 	@echo "Please specify web or win"

win: bin/win/$(GAMENAME).exe
web: bin/web/$(GAMENAME).html

shader2c: bin/shader2c.exe

bin/shader2c.exe: $(TOOL_SHADER2C:%.c=obj/tool/%.o) | bin/
	gcc $^ -o $@

obj/shader.c: $(SHADERS) bin/shader2c.exe | obj/
	bin/shader2c $(SHADERS) obj/shader.c obj/shader.h

obj/tool/%.o: %.c | $(addprefix obj/tool/,$(dir $(TOOL_SHADER2C)))
	gcc -MMD $(CFLAGS) -c $< -o $@ -I../SDL/include -I../assimp/include -I../assimp/build-win/include -Iglad/include -O2

bin/web/$(GAMENAME).html: $(SRCS:%.c=obj/web/%.o) | bin/web/
	emcc $^ -o $@ -L../SDL/build-emcc/ -lSDL3

bin/win/SDL3.dll: ../SDL/build-win/SDL3.dll
	cp ../SDL/build-win/SDL3.dll bin/win/

bin/win/$(GAMENAME).exe: $(SRCS:%.c=obj/win/%.o) | bin/win/ bin/win/SDL3.dll
	g++ $^ -o $@ -L../SDL/build-win/ -L../assimp/build-win/lib -lSDL3 -lassimp -lz

obj/win/%.o: %.c | $(addprefix obj/win/,$(dir $(SRCS))) obj/shader.c
	gcc -MMD $(CFLAGS) -c $< -o $@ -I../SDL/include -I../assimp/include -I../assimp/build-win/include -Iglad/include -O2 -I.

obj/web/%.o: %.c | $(addprefix obj/win/,$(dir $(SRCS)))
	emcc $(CFLAGS) -c $< -o $@ -I../SDL/include -I../assimp/include -I../assimp/build-web/include -Iglad/include

%/:
	mkdir -p $@

clean:
	rm -rf bin
	rm -rf obj

-include $(SRCS:%.c=obj/win/%.d)