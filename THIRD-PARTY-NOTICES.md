# Third-Party Notices — Sledge Distortion

Sledge Distortion is distributed under the GNU General Public License v3.0
(see `LICENSE`). It incorporates the third-party components listed below, each
under its own terms. These notices must accompany every binary and source
distribution of Sledge Distortion.

---

## JUCE — Raw Material Software Limited

Sledge Distortion is built with the [JUCE](https://juce.com) framework
(version 7.0.12).

JUCE 7 is offered under tier-leveled commercial terms **or** under the GNU
General Public License v3. **Sledge Distortion takes the GPL v3 route**, which
is why this project is itself licensed under the GPL v3.

The `juce_audio_basics`, `juce_audio_devices`, `juce_core` and `juce_events`
modules are permissively licensed under the terms of the
[ISC license](https://www.isc.org/licenses/):

```
Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
```

The remaining JUCE modules used here — `juce_audio_processors`,
`juce_audio_plugin_client`, `juce_audio_utils`, `juce_data_structures`,
`juce_dsp`, `juce_graphics`, `juce_gui_basics` and `juce_gui_extra` — are used
under the GNU General Public License v3, a copy of which is in `LICENSE`.

---

## VST3 SDK — Steinberg Media Technologies GmbH

The VST 3 plug-in interface is built from the VST3 SDK bundled with JUCE, which
is licensed under the 3-clause BSD license. Its notice is reproduced here in
full, as that license requires for binary distributions:

```
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2023, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------
```

VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe
and other countries.

---

## Orbitron — The Orbitron Project Authors

`Resources/Orbitron-Regular.ttf` and `Resources/Orbitron-Bold.ttf` are embedded
into the plugin binary. Orbitron is licensed under the SIL Open Font License
1.1, which requires this copyright notice and the license text to accompany
every copy of the font, including embedded copies.

```
Copyright 2018 The Orbitron Project Authors
(https://github.com/theleagueof/orbitron),
with Reserved Font Name: "Orbitron".
```

Orbitron is used unmodified. Note condition 3 below: the Reserved Font Name
may not be used by a *modified* version of the font.

The SIL Open Font License 1.1 does not extend to software that merely embeds
or bundles the font — it governs the font files alone.

```
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007

PREAMBLE The goals of the Open Font License (OFL) are to stimulate
worldwide development of collaborative font projects, to support the font
creation efforts of academic and linguistic communities, and to provide
a free and open framework in which fonts may be shared and improved in
partnership with others.

The OFL allows the licensed fonts to be used, studied, modified and
redistributed freely as long as they are not sold by themselves.
The fonts, including any derivative works, can be bundled, embedded,
redistributed and/or sold with any software provided that any reserved
names are not used by derivative works.  The fonts and derivatives,
however, cannot be released under any other type of license.  The
requirement for fonts to remain under this license does not apply to
any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright
Holder(s) under this license and clearly marked as such.
This may include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the
copyright statement(s).

"Original Version" refers to the collection of Font Software components
as distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting,
or substituting -- in part or in whole --
any of the components of the Original Version, by changing formats or
by porting the Font Software to a new environment.

"Author" refers to any designer, engineer, programmer, technical writer
or other person who contributed to the Font Software.

PERMISSION & CONDITIONS

Permission is hereby granted, free of charge, to any person obtaining a
copy of the Font Software, to use, study, copy, merge, embed, modify,
redistribute, and sell modified and unmodified copies of the Font
Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components,in
   Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled,
   redistributed and/or sold with any software, provided that each copy
   contains the above copyright notice and this license. These can be
   included either as stand-alone text files, human-readable headers or
   in the appropriate machine-readable metadata fields within text or
   binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font
   Name(s) unless explicit written permission is granted by the
   corresponding Copyright Holder. This restriction only applies to the
   primary font name as presented to the users.

4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
   Software shall not be used to promote, endorse or advertise any
   Modified Version, except to acknowledge the contribution(s) of the
   Copyright Holder(s) and the Author(s) or with their explicit written
   permission.

5) The Font Software, modified or unmodified, in part or in whole, must
   be distributed entirely under this license, and must not be distributed
   under any other license. The requirement for fonts to remain under
   this license does not apply to any document created using the Font
   Software.

TERMINATION
This license becomes null and void if any of the above conditions are not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT.  IN NO EVENT SHALL THE
COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM OTHER
DEALINGS IN THE FONT SOFTWARE.
```

---

## Artwork and presets

`Resources/background.png`, `Resources/ui_texture.png`,
`Resources/ui_texture_extreme.png` and `Resources/Logo Title.png`, together
with the factory preset bank, are original works
Copyright © 2026 Boris Miscenco, distributed under the GNU General Public
License v3 as part of this project.
