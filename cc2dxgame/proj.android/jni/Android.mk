LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

$(call import-add-path,$(LOCAL_PATH)/../../cocos2d)
$(call import-add-path,$(LOCAL_PATH)/../../cocos2d/external)
$(call import-add-path,$(LOCAL_PATH)/../../cocos2d/cocos)
$(call import-add-path,$(LOCAL_PATH)/../../cocos2d/cocos/audio/include)

LOCAL_MODULE := MyGame_shared

LOCAL_MODULE_FILENAME := libMyGame

LOCAL_SRC_FILES := hellocpp/main.cpp \
		../../Classes/AppDelegate.cpp \
		../../Classes/HelloWorldScene.cpp \
		../../Classes/CursesTileMap.cpp \
		../../../atrogue-0.3.0/.config \
		../../../atrogue-0.3.0/action.c \
		../../../atrogue-0.3.0/creature.c \
		../../../atrogue-0.3.0/dungeon.c \
		../../../atrogue-0.3.0/init.c \
		../../../atrogue-0.3.0/main.c \
		../../../atrogue-0.3.0/message.c \
		../../../atrogue-0.3.0/object.c \
		../../../atrogue-0.3.0/random.c \
		../../../atrogue-0.3.0/stat.c \
		../../../atrogue-0.3.0/stuff.c \
		../../../PDCurses-3.4/pdcurses/addch.c \
		../../../PDCurses-3.4/pdcurses/addchstr.c \
		../../../PDCurses-3.4/pdcurses/addstr.c \
		../../../PDCurses-3.4/pdcurses/attr.c \
		../../../PDCurses-3.4/pdcurses/beep.c \
		../../../PDCurses-3.4/pdcurses/bkgd.c \
		../../../PDCurses-3.4/pdcurses/border.c \
		../../../PDCurses-3.4/pdcurses/clear.c \
		../../../PDCurses-3.4/pdcurses/color.c \
		../../../PDCurses-3.4/pdcurses/debug.c \
		../../../PDCurses-3.4/pdcurses/delch.c \
		../../../PDCurses-3.4/pdcurses/deleteln.c \
		../../../PDCurses-3.4/pdcurses/deprec.c \
		../../../PDCurses-3.4/pdcurses/getch.c \
		../../../PDCurses-3.4/pdcurses/getstr.c \
		../../../PDCurses-3.4/pdcurses/getyx.c \
		../../../PDCurses-3.4/pdcurses/inch.c \
		../../../PDCurses-3.4/pdcurses/inchstr.c \
		../../../PDCurses-3.4/pdcurses/initscr.c \
		../../../PDCurses-3.4/pdcurses/inopts.c \
		../../../PDCurses-3.4/pdcurses/insch.c \
		../../../PDCurses-3.4/pdcurses/insstr.c \
		../../../PDCurses-3.4/pdcurses/instr.c \
		../../../PDCurses-3.4/pdcurses/kernel.c \
		../../../PDCurses-3.4/pdcurses/keyname.c \
		../../../PDCurses-3.4/pdcurses/mouse.c \
		../../../PDCurses-3.4/pdcurses/move.c \
		../../../PDCurses-3.4/pdcurses/outopts.c \
		../../../PDCurses-3.4/pdcurses/overlay.c \
		../../../PDCurses-3.4/pdcurses/pad.c \
		../../../PDCurses-3.4/pdcurses/panel.c \
		../../../PDCurses-3.4/pdcurses/printw.c \
		../../../PDCurses-3.4/pdcurses/refresh.c \
		../../../PDCurses-3.4/pdcurses/scanw.c \
		../../../PDCurses-3.4/pdcurses/scr_dump.c \
		../../../PDCurses-3.4/pdcurses/scroll.c \
		../../../PDCurses-3.4/pdcurses/slk.c \
		../../../PDCurses-3.4/pdcurses/termattr.c \
		../../../PDCurses-3.4/pdcurses/terminfo.c \
		../../../PDCurses-3.4/pdcurses/touch.c \
		../../../PDCurses-3.4/pdcurses/util.c \
		../../../PDCurses-3.4/pdcurses/window.c \
		../../../PDCurses-3.4/cc2dx/pdccc2dx.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../Classes \
	$(LOCAL_PATH)/../../../atrogue-0.3.0 \
	$(LOCAL_PATH)/../../../PDCurses-3.4 \
	$(LOCAL_PATH)/../../../PDCurses-3.4/cc2dx


# _COCOS_HEADER_ANDROID_BEGIN
# _COCOS_HEADER_ANDROID_END


LOCAL_STATIC_LIBRARIES := cocos2dx_static

# _COCOS_LIB_ANDROID_BEGIN
# _COCOS_LIB_ANDROID_END

include $(BUILD_SHARED_LIBRARY)

$(call import-module,.)

# _COCOS_LIB_IMPORT_ANDROID_BEGIN
# _COCOS_LIB_IMPORT_ANDROID_END
