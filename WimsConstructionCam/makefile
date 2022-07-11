CXX ?= g++

WimsConstructionCam/usr/local/bin/wimconstructioncam: wimconstructioncam.cpp
	mkdir -p $(shell dirname $@)
	$(CXX) -Wno-psabi -O3 -std=c++11 $? -o$@ -lgps

deb: WimsConstructionCam/usr/local/bin/wimconstructioncam WimsConstructionCam/DEBIAN/control WimsConstructionCam/usr/local/lib/systemd/system/wimsconstructioncam.service
	# Set architecture for the resulting .deb to the actually built architecture
	sed -i "s/Architecture: .*/Architecture: $(shell dpkg --print-architecture)/" WimsConstructionCam/DEBIAN/control
	chmod a+x WimsConstructionCam/DEBIAN/postinst WimsConstructionCam/DEBIAN/postrm WimsConstructionCam/DEBIAN/prerm
	dpkg-deb --build WimsConstructionCam

install-deb: deb
	apt install ./WimsConstructionCam.deb

clean:
	-rm -rf WimsConstructionCam/usr/local/bin
	-rm -f WimsConstructionCam.deb

.PHONY: clean deb install-deb
