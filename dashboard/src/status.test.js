import { describe, expect, it } from 'vitest';
import { DEFAULT_COLLECTOR_CONFIG, DEFAULT_NETWORK_THRESHOLDS, DEFAULT_PERCENT_THRESHOLDS, DEFAULT_THRESHOLDS, agentStatus, classifyPercent, groupOverviewAgents, summaryCounts, toDashboardAgents, toDisplayAlerts, } from './status';
function agent(overrides = {}) {
    return {
        agent_id: 'agent-1',
        hostname: 'host-1',
        platform: 'linux',
        curve_public_key_z85: '',
        approved: true,
        rejected: false,
        connected: true,
        maintenance: false,
        maintenance_reason: '',
        maintenance_until: 0,
        collection_interval: 30,
        process_limit: 25,
        first_seen: 1,
        last_seen: 1,
        description: '',
        ...overrides,
    };
}
describe('GIVEN dashboard health thresholds', () => {
    it('WHEN percentages are classified THEN severity increases at warning, degraded, and critical levels', () => {
        expect(classifyPercent(12)).toBe('green');
        expect(classifyPercent(80)).toBe('yellow');
        expect(classifyPercent(90)).toBe('amber');
        expect(classifyPercent(95)).toBe('red');
    });
});
describe('GIVEN agents with component health', () => {
    it('WHEN summary counts are calculated THEN each agent contributes to its host state', () => {
        const agents = [
            { id: 'a', name: 'a', platform: 'linux', approved: true, rejected: false, connected: true, maintenance: false, maintenanceReason: '', maintenanceUntil: 0, collectionInterval: 30, processLimit: 25, thresholds: DEFAULT_THRESHOLDS, collectorConfig: DEFAULT_COLLECTOR_CONFIG, lastSeen: 0, uptime: '1m', group: 'g', groupIds: [], status: 'green', alertColor: 'green', components: [{ key: 'cpu', label: 'CPU', color: 'green', value: '1%', detail: '' }], description: '' },
            { id: 'b', name: 'b', platform: 'linux', approved: true, rejected: false, connected: true, maintenance: false, maintenanceReason: '', maintenanceUntil: 0, collectionInterval: 30, processLimit: 25, thresholds: DEFAULT_THRESHOLDS, collectorConfig: DEFAULT_COLLECTOR_CONFIG, lastSeen: 0, uptime: '1m', group: 'g', groupIds: [], status: 'yellow', alertColor: 'green', components: [{ key: 'cpu', label: 'CPU', color: 'yellow', value: '65%', detail: '' }], description: '' },
            { id: 'c', name: 'c', platform: 'linux', approved: true, rejected: false, connected: true, maintenance: false, maintenanceReason: '', maintenanceUntil: 0, collectionInterval: 30, processLimit: 25, thresholds: DEFAULT_THRESHOLDS, collectorConfig: DEFAULT_COLLECTOR_CONFIG, lastSeen: 0, uptime: '1m', group: 'g', groupIds: [], status: 'red', alertColor: 'red', components: [{ key: 'cpu', label: 'CPU', color: 'red', value: '95%', detail: '' }], description: '' },
        ];
        expect(agentStatus(agents[2])).toBe('red');
        expect(summaryCounts(agents)).toMatchObject({ green: 1, yellow: 1, red: 1 });
    });
});
describe('GIVEN backend agents and metrics', () => {
    it('WHEN dashboard rows are built THEN missing metrics are represented as no-data state', () => {
        const rows = toDashboardAgents([agent()], []);
        expect(rows).toHaveLength(1);
        expect(rows[0].components.find((component) => component.key === 'cpu')?.color).toBe('grey');
        expect(rows[0].components.at(-1)).toMatchObject({ key: 'heartbeat', color: 'green' });
        expect(rows[0].status).toBe('yellow');
    });
    it('WHEN a disconnected agent row is built THEN heartbeat is the final no-data indicator', () => {
        const rows = toDashboardAgents([agent({ connected: false, last_seen: 0 })], []);
        expect(rows[0].components.at(-1)).toMatchObject({
            key: 'heartbeat',
            label: 'Heartbeat',
            color: 'grey',
            value: 'offline',
        });
    });
    it('WHEN maintenance agents are built THEN all indicators are blue', () => {
        const rows = toDashboardAgents([agent({ agent_id: 'agent-maintenance', maintenance: true })], []);
        expect(rows[0].group).toBe('Maintenance Agents');
        expect(rows[0].maintenance).toBe(true);
        expect(rows[0].components.every((component) => component.color === 'blue')).toBe(true);
        expect(rows[0].components.at(-1)).toMatchObject({ key: 'heartbeat', color: 'blue' });
    });
    it('WHEN collector config is supplied THEN dashboard health uses absolute per-agent thresholds and watches', () => {
        const metrics = {
            agent_id: 'agent-1',
            timestamp_ms: 1000,
            metrics: {
                cpu: { usage_percent: 55, num_logical_cores: 8, load_avg_1m: 0 },
                memory: { total_bytes: 1000, used_bytes: 200, usage_percent: 20 },
                disks: [{ device: '/dev/sdb1', mount_point: '/data', filesystem: 'ext4', total_bytes: 1000, used_bytes: 500, usage_percent: 50 }],
                temperatures: [],
                top_processes: [{ pid: 1, name: 'TheWatcherAgent.exe', status: 'running', cpu_percent: 1, memory_rss_bytes: 1, num_threads: 1 }],
                networks: [{ interface_name: 'eth0', bytes_sent_per_sec: 10_000_000, bytes_recv_per_sec: 10_000_000, errors_in: 0, errors_out: 0, drops_in: 0, drops_out: 0, is_up: true }],
                os_name: 'Linux',
                os_version: '6',
                hostname: 'host-1',
                platform: 'linux',
                uptime_seconds: 60,
            },
        };
        const rows = toDashboardAgents([
            agent({
                collector_config: {
                    ...DEFAULT_COLLECTOR_CONFIG,
                    cpu: { ...DEFAULT_PERCENT_THRESHOLDS, warning_percent: 50, degraded_percent: 70, critical_percent: 90 },
                    networks: [{ interface_name: 'eth0', enabled: true, thresholds: DEFAULT_NETWORK_THRESHOLDS }],
                    processes: [{ name: 'TheWatcherAgent.exe', expected_count: 2, enabled: true }],
                },
            }),
        ], [metrics]);
        expect(rows[0].components.find((component) => component.key === 'cpu')).toMatchObject({ color: 'yellow' });
        expect(rows[0].components.find((component) => component.key === 'network')).toMatchObject({ color: 'yellow' });
        expect(rows[0].components.find((component) => component.key === 'processes')).toMatchObject({ color: 'red' });
    });
});
describe('GIVEN overview agents assigned to groups', () => {
    const groups = [
        { group_id: 1, name: 'Production', built_in: false },
        { group_id: 2, name: 'Databases', built_in: false },
    ];
    it('WHEN overview groups are built THEN agents appear under every assigned group with ungrouped agents separated', () => {
        const rows = toDashboardAgents([
            agent({ agent_id: 'agent-prod', hostname: 'prod-1', group_ids: [1] }),
            agent({ agent_id: 'agent-db', hostname: 'db-1', group_ids: [1, 2] }),
            agent({ agent_id: 'agent-free', hostname: 'free-1', group_ids: [] }),
        ], []);
        const grouped = groupOverviewAgents(rows, groups, 'all');
        expect(grouped.map((group) => group.name)).toEqual(['Databases', 'Production', 'Ungrouped']);
        expect(grouped.find((group) => group.name === 'Databases')?.agents.map((row) => row.id)).toEqual(['agent-db']);
        expect(grouped.find((group) => group.name === 'Production')?.agents.map((row) => row.id)).toEqual([
            'agent-db',
            'agent-prod',
        ]);
        expect(grouped.find((group) => group.name === 'Ungrouped')?.agents.map((row) => row.id)).toEqual(['agent-free']);
    });
    it('WHEN a group filter is selected THEN only agents in that group are returned', () => {
        const rows = toDashboardAgents([
            agent({ agent_id: 'agent-prod', hostname: 'prod-1', group_ids: [1] }),
            agent({ agent_id: 'agent-db', hostname: 'db-1', group_ids: [2] }),
            agent({ agent_id: 'agent-free', hostname: 'free-1', group_ids: [] }),
        ], []);
        const grouped = groupOverviewAgents(rows, groups, '2');
        expect(grouped).toHaveLength(1);
        expect(grouped[0].name).toBe('Databases');
        expect(grouped[0].agents.map((row) => row.id)).toEqual(['agent-db']);
    });
});
describe('GIVEN alerts mixed acknowledged and unacknowledged', () => {
    it('WHEN dashboard agents are built THEN alertColor is red only when an unacknowledged alert exists', () => {
        const agents = [
            agent({ agent_id: 'agent-acked' }),
            agent({ agent_id: 'agent-unacked' }),
            agent({ agent_id: 'agent-clean' }),
        ];
        const alerts = [
            { alert_id: 1, agent_id: 'agent-acked', indicator: 'cpu', old_status: 'green', new_status: 'red', message: '', created_at: 1, acknowledged_by: 'admin', acknowledged_at: 999, deleted_at: 0, note: '', escalated_at: 0 },
            { alert_id: 2, agent_id: 'agent-unacked', indicator: 'cpu', old_status: 'green', new_status: 'red', message: '', created_at: 2, acknowledged_by: '', acknowledged_at: 0, deleted_at: 0, note: '', escalated_at: 0 },
        ];
        const rows = toDashboardAgents(agents, [], alerts);
        expect(rows.find((r) => r.id === 'agent-acked')?.alertColor).toBe('green');
        expect(rows.find((r) => r.id === 'agent-unacked')?.alertColor).toBe('red');
        expect(rows.find((r) => r.id === 'agent-clean')?.alertColor).toBe('green');
    });
});
describe('GIVEN alerts and known dashboard agents', () => {
    it('WHEN display alerts are built THEN the hostname is primary and the agent id remains available', () => {
        const rows = toDashboardAgents([agent({ agent_id: 'agent-prod', hostname: 'prod-1' })], []);
        const alerts = [
            {
                alert_id: 1,
                agent_id: 'agent-prod',
                indicator: 'cpu',
                old_status: 'green',
                new_status: 'yellow',
                message: 'cpu changed from green to yellow',
                created_at: 1000,
                acknowledged_by: '',
                acknowledged_at: 0,
                deleted_at: 0,
                note: '',
                escalated_at: 0,
            },
            {
                alert_id: 2,
                agent_id: 'agent-missing',
                indicator: 'memory',
                old_status: 'green',
                new_status: 'red',
                message: 'memory changed from green to red',
                created_at: 2000,
                acknowledged_by: '',
                acknowledged_at: 0,
                deleted_at: 0,
                note: '',
                escalated_at: 0,
            },
        ];
        const displayAlerts = toDisplayAlerts(alerts, rows);
        expect(displayAlerts[0]).toMatchObject({
            agentName: 'prod-1',
            agentId: 'agent-prod',
        });
        expect(displayAlerts[1]).toMatchObject({
            agentName: 'agent-missing',
            agentId: 'agent-missing',
        });
    });
});
