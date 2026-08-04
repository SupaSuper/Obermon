// Scramjet Local native host readiness support.
// SPDX-License-Identifier: AGPL-3.0-or-later
package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"
)

type readyPayload struct {
	HTTP      string    `json:"http"`
	Wisp      string    `json:"wisp"`
	PID       int       `json:"pid"`
	StartedAt time.Time `json:"startedAt"`
}

// writeReadyFile publishes readiness only after both listeners have been bound.
// The temporary-file rename prevents the browser from observing partial JSON.
func writeReadyFile(httpAddr, wispAddr string) error {
	var destination string
	for _, arg := range os.Args[1:] {
		if strings.HasPrefix(arg, "--ready-file=") {
			destination = strings.TrimPrefix(arg, "--ready-file=")
			break
		}
	}
	if destination == "" {
		return nil
	}

	payload, err := json.Marshal(readyPayload{
		HTTP:      httpAddr,
		Wisp:      wispAddr,
		PID:       os.Getpid(),
		StartedAt: time.Now().UTC(),
	})
	if err != nil {
		return fmt.Errorf("encode ready marker: %w", err)
	}
	if err := os.MkdirAll(filepath.Dir(destination), 0o700); err != nil {
		return fmt.Errorf("create ready marker directory: %w", err)
	}

	temporary := destination + ".tmp"
	if err := os.WriteFile(temporary, payload, 0o600); err != nil {
		return fmt.Errorf("write ready marker: %w", err)
	}
	if err := os.Rename(temporary, destination); err != nil {
		_ = os.Remove(temporary)
		return fmt.Errorf("publish ready marker: %w", err)
	}
	return nil
}
