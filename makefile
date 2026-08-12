CXX      = g++
CXXFLAGS = -std=c++17 -O2 -municode -mwindows -Wall -I. -Isrc
LDFLAGS  = -municode -mwindows -lgdi32 -luser32 -lshell32 -lgdiplus -lwinhttp \
           -lcomctl32 -lcomdlg32 -lole32 -loleaut32 -luuid -lshlwapi -ldwmapi

SRCDIR   = src
BUILDDIR = build
OBJS     = $(BUILDDIR)/main.o $(BUILDDIR)/Util.o $(BUILDDIR)/Json.o $(BUILDDIR)/Logger.o \
           $(BUILDDIR)/AppSettings.o $(BUILDDIR)/PromptBuilder.o $(BUILDDIR)/HotkeyManager.o \
           $(BUILDDIR)/TrayIcon.o $(BUILDDIR)/SelectionTracker.o $(BUILDDIR)/OverlayWindow.o \
           $(BUILDDIR)/ToolbarWindow.o $(BUILDDIR)/GdiUtil.o $(BUILDDIR)/CopyImageService.o \
           $(BUILDDIR)/SaveImageService.o $(BUILDDIR)/AiService.o $(BUILDDIR)/ResultWindow.o \
           $(BUILDDIR)/SettingsWindow.o $(BUILDDIR)/ScreenshotSession.o

TARGET   = capture-plus.exe

all: $(TARGET)

$(TARGET): $(OBJS) $(BUILDDIR)/res.o
	$(CXX) $(OBJS) $(BUILDDIR)/res.o -o $(TARGET) $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/Pch.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/res.o: app.rc resource.h | $(BUILDDIR)
	windres app.rc -O coff $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -f $(TARGET)
	rm -rf $(BUILDDIR)

.PHONY: all clean
