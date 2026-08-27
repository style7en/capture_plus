CXX      = g++
CXXFLAGS = -std=c++17 -O2 -municode -mwindows -Wall -MMD -MP -I. -Isrc -Ires
LDFLAGS  = -s -municode -mwindows -lgdi32 -luser32 -lshell32 -lgdiplus -lwinhttp \
           -lcomctl32 -lcomdlg32 -lole32 -loleaut32 -luuid -lshlwapi -ldwmapi

SRCDIR   = src
BUILDDIR = build
OBJS     = $(BUILDDIR)/main.o $(BUILDDIR)/Util.o $(BUILDDIR)/Json.o $(BUILDDIR)/Logger.o \
           $(BUILDDIR)/AppSettings.o $(BUILDDIR)/PromptBuilder.o $(BUILDDIR)/HotkeyManager.o \
           $(BUILDDIR)/TrayIcon.o $(BUILDDIR)/SelectionTracker.o $(BUILDDIR)/OverlayWindow.o \
           $(BUILDDIR)/ToolbarWindow.o $(BUILDDIR)/GdiUtil.o $(BUILDDIR)/CopyImageService.o \
           $(BUILDDIR)/SaveImageService.o $(BUILDDIR)/AiService.o $(BUILDDIR)/ResultWindow.o \
           $(BUILDDIR)/SettingsWindow.o $(BUILDDIR)/ScreenshotSession.o $(BUILDDIR)/MarkdownPreview.o

TARGET   = capture-plus.exe

all: $(TARGET)

$(TARGET): $(OBJS) $(BUILDDIR)/res.o
	$(CXX) $(OBJS) $(BUILDDIR)/res.o -o $(TARGET) $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/Pch.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/res.o: res/app.rc res/resource.h | $(BUILDDIR)
	windres res/app.rc -O coff $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -f $(TARGET)
	rm -rf $(BUILDDIR)
	rm -f tests.exe

TEST_SRCS = tests/main.cpp src/Json.cpp src/Util.cpp src/Logger.cpp src/SelectionTracker.cpp src/HotkeyManager.cpp
TEST_LIBS = -luser32 -lshell32 -lole32 -luuid -lshlwapi

tests.exe: $(TEST_SRCS) src/Pch.h src/Json.h src/Util.h src/Logger.h src/SelectionTracker.h src/HotkeyManager.h
	$(CXX) -std=c++17 -O2 -Wall -I. -Isrc -Ires $(TEST_SRCS) -o tests.exe $(TEST_LIBS)

test: tests.exe
	./tests.exe

-include $(OBJS:.o=.d)

.PHONY: all clean test
