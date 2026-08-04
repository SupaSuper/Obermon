// Scramjet Local warm connection support.
// SPDX-License-Identifier: AGPL-3.0-or-later
package main

import (
	"errors"
	"net"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"sync"
	"time"
)

const (
	warmConnectionLifetime = 12 * time.Second
	speculativeDialTimeout  = 350 * time.Millisecond
	maxWarmPerDestination   = 2
	maxWarmConnections      = 64
)

type warmConnection struct {
	conn      net.Conn
	expiresAt time.Time
}

type warmConnectionPool struct {
	mu      sync.Mutex
	entries map[string][]warmConnection
}

var warmConnections = &warmConnectionPool{entries: make(map[string][]warmConnection)}

func warmConnectionKey(partition, network, address string) string {
	return partition + "\x00" + network + "\x00" + address
}

func (pool *warmConnectionPool) countLocked() int {
	total := 0
	for _, connections := range pool.entries {
		total += len(connections)
	}
	return total
}

func (pool *warmConnectionPool) cleanupLocked(now time.Time) {
	for key, connections := range pool.entries {
		kept := connections[:0]
		for _, item := range connections {
			if now.Before(item.expiresAt) {
				kept = append(kept, item)
			} else {
				_ = item.conn.Close()
			}
		}
		if len(kept) == 0 {
			delete(pool.entries, key)
		} else {
			pool.entries[key] = kept
		}
	}
}

func (pool *warmConnectionPool) preconnect(partition, network, address string, timeout time.Duration) error {
	key := warmConnectionKey(partition, network, address)
	now := time.Now()
	pool.mu.Lock()
	pool.cleanupLocked(now)
	if len(pool.entries[key]) >= maxWarmPerDestination ||
		pool.countLocked() >= maxWarmConnections {
		pool.mu.Unlock()
		return nil
	}
	pool.mu.Unlock()

	conn, err := net.DialTimeout(network, address, timeout)
	if err != nil {
		return err
	}

	pool.mu.Lock()
	defer pool.mu.Unlock()
	pool.cleanupLocked(time.Now())
	if len(pool.entries[key]) >= maxWarmPerDestination ||
		pool.countLocked() >= maxWarmConnections {
		_ = conn.Close()
		return nil
	}
	pool.entries[key] = append(pool.entries[key], warmConnection{
		conn:      conn,
		expiresAt: time.Now().Add(warmConnectionLifetime),
	})
	return nil
}

func (pool *warmConnectionPool) takeOrDial(partition, network, address string, timeout time.Duration) (net.Conn, error) {
	key := warmConnectionKey(partition, network, address)
	pool.mu.Lock()
	pool.cleanupLocked(time.Now())
	connections := pool.entries[key]
	if len(connections) > 0 {
		item := connections[len(connections)-1]
		connections = connections[:len(connections)-1]
		if len(connections) == 0 {
			delete(pool.entries, key)
		} else {
			pool.entries[key] = connections
		}
		pool.mu.Unlock()
		_ = item.conn.SetDeadline(time.Time{})
		return item.conn, nil
	}
	pool.mu.Unlock()
	return net.DialTimeout(network, address, timeout)
}

func normalizedPartition(value string) string {
	value = strings.TrimSpace(value)
	if value == "" || len(value) > 128 {
		return "default"
	}
	for _, character := range value {
		if !(character == '-' || character == '_' ||
			character >= '0' && character <= '9' ||
			character >= 'a' && character <= 'z' ||
			character >= 'A' && character <= 'Z') {
			return "default"
		}
	}
	return value
}

func destinationAddress(raw string) (string, error) {
	destination, err := url.Parse(raw)
	if err != nil || destination.Hostname() == "" {
		return "", errors.New("invalid destination URL")
	}
	if destination.Scheme != "http" && destination.Scheme != "https" {
		return "", errors.New("unsupported destination scheme")
	}
	port := destination.Port()
	if port == "" {
		if destination.Scheme == "https" {
			port = "443"
		} else {
			port = "80"
		}
	}
	if parsedPort, err := strconv.ParseUint(port, 10, 16); err != nil || parsedPort == 0 {
		return "", errors.New("invalid destination port")
	}
	return net.JoinHostPort(destination.Hostname(), port), nil
}

// handlePreconnect is invoked by the local HTTP handler. Connections are keyed
// by the browser-generated mediation token and can only be consumed by a Wisp
// session presenting the same token.
func handlePreconnect(w http.ResponseWriter, r *http.Request) bool {
	if r.URL.Path != "/preconnect" {
		return false
	}
	if !originAllowed(r.Header.Get("Origin")) {
		http.Error(w, "origin not allowed", http.StatusForbidden)
		return true
	}
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return true
	}
	partition := normalizedPartition(r.URL.Query().Get("partition"))
	if partition == "default" {
		http.Error(w, "valid mediation partition required", http.StatusForbidden)
		return true
	}
	address, err := destinationAddress(r.URL.Query().Get("destination"))
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return true
	}
	if err := warmConnections.preconnect(
		partition, "tcp", address, speculativeDialTimeout,
	); err != nil {
		http.Error(w, "preconnect failed", http.StatusBadGateway)
		return true
	}
	w.Header().Set("Cache-Control", "no-store")
	w.WriteHeader(http.StatusNoContent)
	return true
}
