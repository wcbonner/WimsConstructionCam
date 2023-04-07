CXX ?= g++
CXXFLAGS += -D_USE_GPSD -std=c++17

WimsConstructionCam/usr/local/bin/wimsconstructioncam: wimsconstructioncam.o
	mkdir -p $(shell dirname $@)
	$(CXX) $? -o$@ -lgps -lgd -lexif

wimsconstructioncam.o: wimsconstructioncam.cpp makefile
	$(CXX) -c -Wno-psabi -O3 $(CXXFLAGS) $? -o$@

deb: WimsConstructionCam/usr/local/bin/wimsconstructioncam WimsConstructionCam/DEBIAN/control WimsConstructionCam/usr/local/lib/systemd/system/wimsconstructioncam.service
	# Set architecture for the resulting .deb to the actually built architecture
	sed -i "s/Architecture: .*/Architecture: $(shell dpkg --print-architecture)/" WimsConstructionCam/DEBIAN/control
	chmod a+x WimsConstructionCam/DEBIAN/postinst WimsConstructionCam/DEBIAN/postrm WimsConstructionCam/DEBIAN/prerm
	dpkg-deb --build WimsConstructionCam
	dpkg-name --overwrite WimsConstructionCam.deb

clean:
	-rm -rf WimsConstructionCam/usr/local/bin
	rm wimsconstructioncam.o
	git restore WimsConstructionCam/DEBIAN/control

.PHONY: clean deb install-deb
