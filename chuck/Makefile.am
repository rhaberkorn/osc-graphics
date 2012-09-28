
dist_chuck_DATA = OSCGraphics.ck OSCGraphicsPort.ck OSCGraphicsLayer.ck \
		  OSCGraphicsBox.ck OSCGraphicsImage.ck OSCGraphicsVideo.ck
nodist_chuck_DATA = lib.ck

CLEANFILES = lib.ck
EXTRA_DIST = lib.ck.in

lib.ck : lib.ck.in
	@SED@ 's|@chuckdir[@]|$(chuckdir)|g' $< >$@
