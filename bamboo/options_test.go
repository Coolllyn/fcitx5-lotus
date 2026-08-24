/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
package main

import (
	"testing"

	"bamboo-core"
)

func newOptEngine(flags uint, dict map[string]bool, spellCheckWithDicts bool) *FcitxBambooEngine {
	e := newTestEngine(dict, spellCheckWithDicts)
	e.preeditor = bamboo.NewEngine(bamboo.ParseInputMethod(bamboo.InputMethodDefinitions, "Telex"), flags)
	return e
}

func TestW2UModes(t *testing.T) {
	cases := []struct {
		name  string
		flags uint
		mode  int // -1 = don't call SetW2UMode
		keys  string
		want  string
	}{
		{"default", bamboo.EstdFlags, -1, "w", "ư"},
		{"default-bw", bamboo.EstdFlags, -1, "bw", "bư"},
		{"flag-off", bamboo.EstdFlags &^ bamboo.Ew2uEnabled, -1, "w", "w"},
		{"flag-off-bw", bamboo.EstdFlags &^ bamboo.Ew2uEnabled, -1, "bw", "bw"},
		{"disabled", bamboo.EstdFlags, bamboo.W2uDisabled, "w", "w"},
		{"nonstart", bamboo.EstdFlags, bamboo.W2uNonStart, "w", "w"},
		{"nonstart-bw", bamboo.EstdFlags, bamboo.W2uNonStart, "bw", "bư"},
		{"everywhere", bamboo.EstdFlags, bamboo.W2uEverywhere, "w", "ư"},
	}
	for _, c := range cases {
		e := newOptEngine(c.flags, nil, false)
		if c.mode >= 0 {
			e.preeditor.SetW2UMode(c.mode)
		}
		typeKeys(e, c.keys)
		if e.preeditText != c.want {
			t.Errorf("w2u %s: type [%s] preedit got [%s] expected [%s]", c.name, c.keys, e.preeditText, c.want)
		}
	}
	// "aw" -> ă takes precedence over w->ư on the default engine.
	e := newOptEngine(bamboo.EstdFlags, nil, false)
	typeKeys(e, "aw")
	if e.preeditText != "ă" {
		t.Errorf("w2u default: type [aw] preedit got [%s] expected [ă]", e.preeditText)
	}
}

func TestOptionMatrixAutoRestoreSpellCheck(t *testing.T) {
	dict := map[string]bool{"chào": true}
	type combo struct {
		autoRestore     bool
		withDicts       bool
		wantChaofCommit string
		wantBoawjm      string
	}
	matrix := []combo{
		{true, true, "chào ", "boawjm "},
		{true, false, "chào ", "boặm "},
		{false, true, "chào ", "boặm "},
		{false, false, "chào ", "boặm "},
	}
	for _, c := range matrix {
		e := newTestEngine(dict, c.withDicts)
		e.autoNonVnRestore = c.autoRestore
		typeKeys(e, "chaof")
		e.preeditProcessKeyEvent(FcitxSpace, 0)
		if e.commitText != c.wantChaofCommit {
			t.Errorf("matrix auto=%v dict=%v: [chaof] commit got [%s] expected [%s]",
				c.autoRestore, c.withDicts, e.commitText, c.wantChaofCommit)
		}

		e = newTestEngine(dict, c.withDicts)
		e.autoNonVnRestore = c.autoRestore
		typeKeys(e, "boawjm")
		e.preeditProcessKeyEvent(FcitxSpace, 0)
		if e.commitText != c.wantBoawjm {
			t.Errorf("matrix auto=%v dict=%v: [boawjm] commit got [%s] expected [%s]",
				c.autoRestore, c.withDicts, e.commitText, c.wantBoawjm)
		}
	}
}

func TestSpellCheckWithDictsDirect(t *testing.T) {
	// dict hit (lowercase key as produced by NewDictionary) -> no fallback
	e := newTestEngine(map[string]bool{"chào": true}, true)
	typeKeys(e, "chaof")
	if e.mustFallbackToEnglish() {
		t.Errorf("mustFallbackToEnglish for [chaof] with dict hit got [true] expected [false]")
	}
	// dict miss -> fallback
	e = newTestEngine(map[string]bool{"chào": true}, true)
	typeKeys(e, "boawjm")
	if !e.mustFallbackToEnglish() {
		t.Errorf("mustFallbackToEnglish for [boawjm] with dict miss got [false] expected [true]")
	}
	// empty dict -> everything misses
	e = newTestEngine(map[string]bool{}, true)
	typeKeys(e, "chaof")
	if !e.mustFallbackToEnglish() {
		t.Errorf("mustFallbackToEnglish for [chaof] with empty dict got [false] expected [true]")
	}
}

func TestAutoNonVnRestoreDirect(t *testing.T) {
	e := newTestEngine(map[string]bool{}, true)
	e.autoNonVnRestore = false
	typeKeys(e, "boawjm")
	if e.mustFallbackToEnglish() {
		t.Errorf("autoNonVnRestore=false: mustFallbackToEnglish got [true] expected [false] (early return)")
	}
	e = newTestEngine(map[string]bool{}, true)
	typeKeys(e, "boawjm")
	if !e.mustFallbackToEnglish() {
		t.Errorf("autoNonVnRestore=true: mustFallbackToEnglish got [false] expected [true]")
	}
}

func TestModernStyle(t *testing.T) {
	e := newOptEngine(bamboo.EstdFlags, nil, false)
	typeKeys(e, "hoaf")
	if e.preeditText != "hòa" {
		t.Errorf("std tone style: hoaf preedit got [%s] expected [hòa]", e.preeditText)
	}
	e = newOptEngine(bamboo.EstdFlags&^bamboo.EstdToneStyle, nil, false)
	typeKeys(e, "hoaf")
	if e.preeditText != "hoà" {
		t.Errorf("modern tone style: hoaf preedit got [%s] expected [hoà]", e.preeditText)
	}
}

func TestBracketTransform(t *testing.T) {
	cases := []struct {
		keys string
		want string
	}{
		{"[", "ơ"},
		{"t[", "tơ"},
		{"]", "ư"},
		{"t]", "tư"},
	}
	for _, c := range cases {
		e := newTestEngine(nil, false)
		e.preeditor.SetBracketTransformMode(bamboo.BracketTransformEverywhere)
		typeKeys(e, c.keys)
		if e.preeditText != c.want {
			t.Errorf("bracket: type [%s] preedit got [%s] expected [%s]", c.keys, e.preeditText, c.want)
		}
	}
}

func TestFreeMarkingFlag(t *testing.T) {
	be, ok := newTestEngine(nil, false).preeditor.(*bamboo.BambooEngine)
	if !ok {
		t.Skip("preeditor is not *bamboo.BambooEngine")
	}
	if be.GetFlag(0)&bamboo.EfreeToneMarking == 0 {
		t.Errorf("default EstdFlags: EfreeToneMarking missing from flags")
	}
	be2 := newOptEngine(bamboo.EstdFlags&^bamboo.EfreeToneMarking, nil, false).preeditor.(*bamboo.BambooEngine)
	if be2.GetFlag(0)&bamboo.EfreeToneMarking != 0 {
		t.Errorf("EstdFlags&^EfreeToneMarking: flag still set")
	}
	be2.SetFlag(bamboo.EstdFlags | bamboo.EfreeToneMarking)
	if be2.GetFlag(0)&bamboo.EfreeToneMarking == 0 {
		t.Errorf("SetFlag(EstdFlags|EfreeToneMarking): flag not present")
	}
}
