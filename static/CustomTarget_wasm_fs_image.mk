# vim: set noet sw=4 ts=4:
# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CustomTarget_CustomTarget,static/wasm_fs_image))

$(call gb_CustomTarget_get_workdir,static/wasm_fs_image): \
    $(call gb_InstallModule_get_target,scp2/ooo) \

# file_packager.py supports renaming files by using "<src>@<dest>" input with all
# @ escaped as @@ in both paths. Decoding this "encoding" in Makefile seems hard.
#
# Currently WASM simply assumes the image has the same layout then instdir. The
# command is run from $(BUILDDIR), so everything from $(BUILDDIR) can be just
# included "as it". Files from $(SRCDIR) are converted to the '@' syntax.
#
# Easiest workaround for most other limitations is probably to hack the filenames
# in soffice.data.js.metadata after the image generation or manually add additional
# commandline entries to the file_packager.py call.
#
gb_wasm_image_filelist := \
    $(call gb_UnoApi_get_target,offapi) \
    $(call gb_UnoApi_get_target,udkapi) \
    $(INSTROOT)/$(LIBO_ETC_FOLDER)/$(call gb_Helper_get_rcfile,fundamental) \
    $(INSTROOT)/$(LIBO_ETC_FOLDER)/$(call gb_Helper_get_rcfile,louno) \
    $(INSTROOT)/$(LIBO_ETC_FOLDER)/services/services.rdb \
    $(INSTROOT)/$(LIBO_URE_ETC_FOLDER)/$(call gb_Helper_get_rcfile,uno) \
    $(INSTROOT)/$(LIBO_URE_MISC_FOLDER)/services.rdb \

wasm_fs_image_WORKDIR := $(call gb_CustomTarget_get_workdir,static/wasm_fs_image)

# we just need data.js.link at link time, which is equal to soffice.data.js
$(call gb_CustomTarget_get_target,static/wasm_fs_image): \
    $(wasm_fs_image_WORKDIR)/soffice.data \
    $(wasm_fs_image_WORKDIR)/soffice.data.js.link \
    $(wasm_fs_image_WORKDIR)/soffice.data.js.metadata \

$(wasm_fs_image_WORKDIR)/soffice.data $(wasm_fs_image_WORKDIR)/soffice.data.js : $(wasm_fs_image_WORKDIR)/soffice.data.js.metadata

$(wasm_fs_image_WORKDIR)/soffice.data.js.link: $(wasm_fs_image_WORKDIR)/soffice.data.js
	cp $^ $^.tmp
	$(call gb_Helper_replace_if_different_and_touch,$^.tmp,$@)

$(wasm_fs_image_WORKDIR)/soffice.data.filelist: $(gb_wasm_image_filelist) | $(wasm_fs_image_WORKDIR)/.dir
	$(call gb_Output_announce,$(subst $(BUILDDIR)/,,$(wasm_fs_image_WORKDIR)/soffice.data.filelist),$(true),GEN,2)
	TEMPFILE=$(call gb_var2file,$(shell $(gb_MKTEMP)),1,\
	    $(subst @,@@,$(subst $(BUILDDIR)/,,$(filter $(BUILDDIR)%,$^))) \
	    $(foreach item,$(filter-out $(BUILDDIR)%,$^),$(subst @,@@,$(item))@$(subst @,@@,$(subst $(SRCDIR)/,,$(item))))) \
	&& mv $$TEMPFILE $@

# Unfortunatly the file packager just allows a cmdline file list, but all paths are
# relative to $(BUILDDIR), so we won't run out of cmdline space that fast...
$(wasm_fs_image_WORKDIR)/soffice.data.js.metadata: $(wasm_fs_image_WORKDIR)/soffice.data.filelist \
	$(call gb_Output_announce,$(subst $(BUILDDIR)/,,$(wasm_fs_image_WORKDIR)/soffice.data),$(true),GEN,2)
	$(EMSDK_FILE_PACKAGER) $(wasm_fs_image_WORKDIR)/soffice.data --preload $(shell cat $^) --js-output=$(wasm_fs_image_WORKDIR)/soffice.data.js --separate-metadata \
	    || rm -f $(wasm_fs_image_WORKDIR)/soffice.data.js $(wasm_fs_image_WORKDIR)/soffice.data $(wasm_fs_image_WORKDIR)/soffice.data.js.metadata

# vim: set noet sw=4:
