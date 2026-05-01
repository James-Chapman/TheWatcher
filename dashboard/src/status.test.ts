import { describe, expect, it } from 'vitest';
import { agentStatus, classifyPercent, summaryCounts, toDashboardAgents } from './status';
import type { AgentRecord, DashboardAgent, MetricsSnapshot } from './models';

describe('GIVEN dashboard health thresholds', () => {
  it('WHEN percentages are classified THEN severity increases at warning, degraded, and critical levels', () => {
    expect(classifyPercent(12)).toBe('green');
    expect(classifyPercent(60)).toBe('yellow');
    expect(classifyPercent(75)).toBe('orange');
    expect(classifyPercent(90)).toBe('red');
  });
});

describe('GIVEN agents with component health', () => {
  it('WHEN summary counts are calculated THEN each agent contributes to its worst component state', () => {
    const agents: DashboardAgent[] = [
      { id: 'a', name: 'a', platform: 'linux', approved: true, rejected: false, connected: true, maintenance: false, collectionInterval: 30, processLimit: 25, lastSeen: 0, uptime: '1m', group: 'g', components: [{ key: 'cpu', label: 'CPU', color: 'green', value: '1%', detail: '' }] },
      { id: 'b', name: 'b', platform: 'linux', approved: true, rejected: false, connected: true, maintenance: false, collectionInterval: 30, processLimit: 25, lastSeen: 0, uptime: '1m', group: 'g', components: [{ key: 'cpu', label: 'CPU', color: 'yellow', value: '65%', detail: '' }] },
      { id: 'c', name: 'c', platform: 'linux', approved: true, rejected: false, connected: true, maintenance: false, collectionInterval: 30, processLimit: 25, lastSeen: 0, uptime: '1m', group: 'g', components: [{ key: 'cpu', label: 'CPU', color: 'red', value: '95%', detail: '' }] },
    ];

    expect(agentStatus(agents[2])).toBe('red');
    expect(summaryCounts(agents)).toMatchObject({ green: 1, yellow: 1, red: 1 });
  });
});

describe('GIVEN backend agents and metrics', () => {
  it('WHEN dashboard rows are built THEN missing metrics are represented as maintenance state', () => {
    const now = 1_700_000_000_000;
    const agents: AgentRecord[] = [
      {
        agent_id: 'agent-1',
        hostname: 'host-1',
        platform: 'linux',
        curve_public_key_z85: '',
        approved: true,
        rejected: false,
        connected: false,
        maintenance: false,
        collection_interval: 30,
        process_limit: 25,
        first_seen: now - 1000,
        last_seen: now - 1000,
      },
    ];
    const snapshots: MetricsSnapshot[] = [];

    const rows = toDashboardAgents(agents, snapshots, now);

    expect(rows).toHaveLength(1);
    expect(rows[0].components.find((component) => component.key === 'cpu')?.color).toBe('blue');
  });

  it('WHEN rejected agents are built THEN they are grouped and marked as rejected enrollment', () => {
    const now = 1_700_000_000_000;
    const agents: AgentRecord[] = [
      {
        agent_id: 'agent-rejected',
        hostname: 'host-rejected',
        platform: 'linux',
        curve_public_key_z85: '',
        approved: false,
        rejected: true,
        connected: false,
        maintenance: false,
        collection_interval: 30,
        process_limit: 25,
        first_seen: now - 1000,
        last_seen: now - 1000,
      },
    ];

    const rows = toDashboardAgents(agents, [], now);

    expect(rows[0].group).toBe('Rejected Agents');
    expect(rows[0].components.find((component) => component.key === 'approval')?.value).toBe('rejected');
  });

  it('WHEN maintenance agents are built THEN they are grouped and marked as maintenance state', () => {
    const now = 1_700_000_000_000;
    const agents: AgentRecord[] = [
      {
        agent_id: 'agent-maintenance',
        hostname: 'host-maintenance',
        platform: 'linux',
        curve_public_key_z85: '',
        approved: true,
        rejected: false,
        connected: true,
        maintenance: true,
        collection_interval: 30,
        process_limit: 25,
        first_seen: now - 1000,
        last_seen: now - 1000,
      },
    ];

    const rows = toDashboardAgents(agents, [], now);

    expect(rows[0].group).toBe('Maintenance Agents');
    expect(rows[0].maintenance).toBe(true);
    expect(rows[0].components.find((component) => component.key === 'heartbeat')?.value).toBe('maintenance');
  });
});
