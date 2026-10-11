/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * TestDumpGoldenCorpus writes the golden corpus the C++ port is verified against:
 * test/golden/{charset,engine_core,key_processor}.tsv.
 *
 * It is skipped unless LOTUS_DUMP_GOLDEN=1, so a plain `go test ./...` never writes files.
 * Regenerate with:
 *
 *     cd bamboo && LOTUS_DUMP_GOLDEN=1 go test -run TestDumpGoldenCorpus -count=1
 *
 * Kept 1:1 with the C++ port: the corpus drives the very Go implementation the port replaces.
 */
package main

import (
	"encoding/hex"
	"fmt"
	"math/rand/v2"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"bamboo-core"
)

const (
	goldenSeedDefault        = 20261007
	randomCasesDefault       = 2000
	maxSequenceLengthDefault = 12
)

// goldenSeed, randomCases and maxSequenceLength are configurable so a local sweep can drive the very
// same corpus builder with a much larger sample. With no environment variable set they keep the
// values the committed test/golden files were generated with, so
// `LOTUS_DUMP_GOLDEN=1 go test -run TestDumpGoldenCorpus` stays byte for byte reproducible.
var (
	goldenSeed        = uint64(envInt("LOTUS_GOLDEN_SEED", goldenSeedDefault))
	randomCases       = envInt("LOTUS_GOLDEN_RANDOM_COUNT", randomCasesDefault)
	maxSequenceLength = envInt("LOTUS_GOLDEN_MAX_LENGTH", maxSequenceLengthDefault)
)

// envInt reads a positive integer from the environment, falling back on anything unset or invalid.
func envInt(name string, fallback int) int {
	value := os.Getenv(name)
	if value == "" {
		return fallback
	}
	var parsed int
	if _, err := fmt.Sscanf(value, "%d", &parsed); err != nil || parsed <= 0 {
		return fallback
	}
	return parsed
}

// step is one keystroke of a corpus sequence. mode is the ProcessKey mode for the core engine, state
// is the modifier mask handed to the key processor wrapper (the two must not share a field: the core
// interprets the value as a Mode, the wrapper as Fcitx modifier bits).
type step struct {
	key   uint32
	mode  bamboo.Mode
	state uint32
}

// sequence is one independent scenario: a fresh engine, a flag set and a list of keys.
type sequence struct {
	id    string
	flags uint
	w2u   int
	brk   int
	opts  uint32
	steps []step
}

const (
	optAutoNonVnRestore    uint32 = 1 << 0
	optDdFreeStyle         uint32 = 1 << 1
	optSpellCheckWithDicts uint32 = 1 << 2
	optMacroEnabled        uint32 = 1 << 3
)

var curatedSequences = []string{
	"dd", "ddd", "dddd", "DuOwOwNgF", "duwowngf", "tieengs vieejt", "xin chaof", "muwowif", "thuowngf",
	"nguwowif", "uow", "uoww", "uwo", "uowng", "w", "ww", "ow", "oow", "uw", "uww", "aw", "aaw",
	"ee", "eee", "oo", "ooo", "dw", "[", "]", "{", "}", "[o", "{o", "dDuOwOwNgF", "f]", "z", "s",
	"đ", "đđ", "nghieeng", "ngieng", "qu", "gi", "Khoong", "ddaay", " dduwowngf", "a.b", "w2u", "uwow",
	// Rare branches: bracket transform right before a word boundary, the "uwo" shortcut tails, the
	// built in macros of dumpKeyProcessor, Tab, and a bracket key after an already transformed one.
	"[o]", "o]", "d[o", "{o}", "g20ư]", "ưow", "uơw", "uơo", "w]", "w{", "vn ", "ko.", "VN ", "Vn.", "\t",
	"a\tb", "oow2", "uwo2", "đường",
}

var curatedKeyValues = []uint32{'[', ']', '{', '}', 'w', 'o', 'u', 'd', 'a', 's', 'f', 'z', ' '}

// stateMasks are the modifier masks the wrapper accepts: isValidState only rejects Ctrl, Mod1,
// Super, Hyper and Meta, so plain, Lock, Shift and Shift+Lock are the ones worth replaying.
var stateMasks = []uint32{0, FcitxLockMask, FcitxShiftMask, FcitxShiftMask | FcitxLockMask}

func randomFlags(rnd *rand.Rand) (uint, int, int) {
	flags := bamboo.EstdFlags
	if rnd.IntN(4) == 0 {
		flags &^= bamboo.EfreeToneMarking
	}
	if rnd.IntN(4) == 0 {
		flags &^= bamboo.EstdToneStyle
	}
	w2u := []int{bamboo.W2uFollowFlags, bamboo.W2uDisabled, bamboo.W2uNonStart, bamboo.W2uEverywhere}[rnd.IntN(4)]
	brk := []int{bamboo.BracketTransformFollowFlags, bamboo.BracketTransformDisabled, bamboo.BracketTransformNonStart, bamboo.BracketTransformEverywhere}[rnd.IntN(4)]
	if w2u == bamboo.W2uFollowFlags && rnd.IntN(2) == 0 {
		flags |= bamboo.Ew2uEnabled
	}
	if brk == bamboo.BracketTransformFollowFlags && rnd.IntN(2) == 0 {
		flags |= bamboo.EbracketTransformEnabled
	}
	return flags, w2u, brk
}

func randomOptions(rnd *rand.Rand) uint32 {
	var opts uint32
	if rnd.IntN(4) != 0 {
		opts |= optAutoNonVnRestore
	}
	if rnd.IntN(2) == 0 {
		opts |= optDdFreeStyle
	}
	if rnd.IntN(3) == 0 {
		opts |= optSpellCheckWithDicts
	}
	if rnd.IntN(4) == 0 {
		opts |= optMacroEnabled
	}
	// Spell checking stays deterministic: dumpKeyProcessor fills the fixed built in word list below,
	// and the C++ replay builds the same three words, so no external dictionary file is ever read.
	return opts
}

// buildCorpus returns the curated and random sequences, in a fixed order.
func buildCorpus() []sequence {
	var corpus []sequence
	for i, text := range curatedSequences {
		flags, w2u, brk := randomFlags(rand.New(rand.NewPCG(goldenSeed, uint64(i)+1)))
		state := stateMasks[i%len(stateMasks)]
		var steps []step
		mode := bamboo.VietnameseMode
		switch i % 5 {
		case 1:
			mode = bamboo.EnglishMode
		case 4:
			mode = bamboo.VietnameseMode | bamboo.InReverseOrder
		}
		for _, r := range text {
			steps = append(steps, step{key: uint32(r), mode: mode, state: state})
		}
		// Every fourth curated sequence also exercises backspace in the middle of a word.
		if i%4 == 0 && len(steps) > 1 {
			position := len(steps) / 2
			steps = append(steps[:position], append([]step{{key: FcitxBackSpace, state: state}}, steps[position:]...)...)
		}
		corpus = append(corpus, sequence{id: fmt.Sprintf("curated-%02d", i), flags: flags, w2u: w2u, brk: brk, opts: randomOptions(rand.New(rand.NewPCG(goldenSeed+1, uint64(i)+1))), steps: steps})
	}

	rnd := rand.New(rand.NewPCG(goldenSeed, 2))
	alphabet := []rune("aăâbcdđeêghiklmnoôơpqrstuưvxywzsfrxj0123456789[]{} '.,")
	for i := 0; i < randomCases; i++ {
		length := 1 + rnd.IntN(maxSequenceLength)
		flags, w2u, brk := randomFlags(rnd)
		opts := randomOptions(rnd)
		state := stateMasks[rnd.IntN(len(stateMasks))]
		var steps []step
		for j := 0; j < length; j++ {
			mode := bamboo.VietnameseMode
			switch rnd.IntN(8) {
			case 0:
				mode = bamboo.EnglishMode
			case 1:
				mode = bamboo.VietnameseMode | bamboo.ToneLess
			case 2:
				mode = bamboo.VietnameseMode | bamboo.InReverseOrder
			case 3:
				mode = bamboo.VietnameseMode | bamboo.PunctuationMode
			}
			steps = append(steps, step{key: uint32(alphabet[rnd.IntN(len(alphabet))]), mode: mode, state: state})
		}
		// Every third sequence also exercises backspace in the middle of a word.
		if i%3 == 0 && len(steps) > 1 {
			position := rnd.IntN(len(steps))
			steps = append(steps[:position], append([]step{{key: FcitxBackSpace, state: state}}, steps[position:]...)...)
		}
		corpus = append(corpus, sequence{id: fmt.Sprintf("random-%04d", i), flags: flags, w2u: w2u, brk: brk, opts: opts, steps: steps})
	}
	return corpus
}

func writeFile(t *testing.T, path string, header string, body *strings.Builder) {
	t.Helper()
	content := header + body.String()
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatalf("write %s: %v", path, err)
	}
	t.Logf("wrote %s (%d bytes)", path, len(content))
}

// dumpCharset writes every charset definition value byte for byte.
func dumpCharset(t *testing.T, path string) {
	t.Helper()
	goldenRunes := append([]rune(bamboo.Vowels), []rune("dDđĐaAơƠưƯăĂâÂêÊôÔ")...)
	var body strings.Builder
	for _, name := range bamboo.GetCharsetNames() {
		for _, r := range goldenRunes {
			encoded := bamboo.Encode(name, string(r))
			fmt.Fprintf(&body, "%s\tU+%04X\t%s\n", name, r, hex.EncodeToString([]byte(encoded)))
		}
	}
	writeFile(t, path, "# charset\tcodepoint\thexEncodedBytes\n", &body)
}

// dumpEngineCore writes the behaviour of the bare engine (bamboo-core).
func dumpEngineCore(t *testing.T, path string) {
	t.Helper()
	var body strings.Builder
	for _, seq := range buildCorpus() {
		engine := bamboo.NewEngine(bamboo.ParseInputMethod(bamboo.InputMethodDefinitions, "Telex"), seq.flags)
		engine.SetW2UMode(seq.w2u)
		engine.SetBracketTransformMode(seq.brk)
		for index, s := range seq.steps {
			engine.ProcessKey(rune(s.key), bamboo.Mode(s.mode))
			processed := engine.GetProcessedString(bamboo.VietnameseMode | bamboo.FullText)
			valid := "0"
			if engine.IsValid(true) {
				valid = "1"
			}
			fmt.Fprintf(&body, "%s\t%d\t%04X\t%d\t%d\t%d\t%d\t%s\t%s\n",
				seq.id, index, s.key, s.mode, seq.flags, seq.w2u, seq.brk, hex.EncodeToString([]byte(processed)), valid)
		}
	}
	writeFile(t, path, "# case\tstep\tkeyHex\tmode\tflags\tw2u\tbracket\tprocessedHex\tisValid\n", &body)
}

// dumpKeyProcessor writes the behaviour of the fcitx wrapper (FcitxBambooEngine).
func dumpKeyProcessor(t *testing.T, path string) {
	t.Helper()
	var body strings.Builder
	for _, seq := range buildCorpus() {
		dict := map[string]bool{"vietnam": true, "nguoi": true, "duong": true}
		engine := newTestEngine(dict, seq.opts&optSpellCheckWithDicts != 0)
		engine.autoNonVnRestore = seq.opts&optAutoNonVnRestore != 0
		engine.ddFreeStyle = seq.opts&optDdFreeStyle != 0
		engine.macroEnabled = seq.opts&optMacroEnabled != 0
		if engine.macroEnabled {
			engine.macroTable = &MacroTable{mTable: map[string]string{"vn": "Việt Nam", "ko": "không"}}
		}
		engine.preeditor.SetFlag(seq.flags)
		engine.preeditor.SetW2UMode(seq.w2u)
		engine.preeditor.SetBracketTransformMode(seq.brk)
		for index, s := range seq.steps {
			state := s.state
			accepted := engine.preeditProcessKeyEvent(s.key, state)
			accept := "0"
			if accepted {
				accept = "1"
			}
			commit := engine.commitText
			engine.commitText = ""
			fmt.Fprintf(&body, "%s\t%d\t%04X\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n",
				seq.id, index, s.key, state, seq.opts, seq.flags, seq.w2u, seq.brk, accept,
				hex.EncodeToString([]byte(engine.preeditText)), hex.EncodeToString([]byte(commit)))
		}
	}
	writeFile(t, path, "# case\tstep\tkeyHex\tstate\topts\tflags\tw2u\tbracket\taccepted\tpreeditHex\tcommitHex\n", &body)
}

func TestDumpGoldenCorpus(t *testing.T) {
	if os.Getenv("LOTUS_DUMP_GOLDEN") != "1" {
		t.Skip("set LOTUS_DUMP_GOLDEN=1 to regenerate test/golden")
	}
	outDir := filepath.Join("..", "test", "golden")
	if override := os.Getenv("LOTUS_GOLDEN_OUT_DIR"); override != "" {
		outDir = override
	}
	if err := os.MkdirAll(outDir, 0o755); err != nil {
		t.Fatal(err)
	}
	dumpCharset(t, filepath.Join(outDir, "charset.tsv"))
	dumpEngineCore(t, filepath.Join(outDir, "engine_core.tsv"))
	dumpKeyProcessor(t, filepath.Join(outDir, "key_processor.tsv"))
}
