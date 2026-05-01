import type {
  AgentRecord,
  AlertRecord,
  AgentThresholds,
  ComponentHealth,
  DashboardAgent,
  GroupRecord,
  HealthColor,
  MetricsSnapshot,
  SystemMetrics,
} from './models';

export type SummaryKey = 'green' | 'yellow' | 'amber' | 'red' | 'blue' | 'offline';
export type OverviewGroupFilter = 'all' | 'ungrouped' | string;

export interface OverviewAgentGroup {
  id: string;
  name: string;
  agents: DashboardAgent[];
}

export interface DisplayAlert extends AlertRecord {
  agentName: string;
  agentId: string;
}

const COLOR_RANK: Record<HealthColor, number> = {
  green: 0,
  grey: 1,
  yellow: 2,
  amber: 3,
  red: 4,
  blue: 5,
};

export const DEFAULT_THRESHOLDS: AgentThresholds = {
  cpu: { warning_pct_of_avg: 125, degraded_pct_of_avg: 150, critical_pct_of_avg: 200 },
  memory: { warning_pct_of_avg: 125, degraded_pct_of_avg: 150, critical_pct_of_avg: 200 },
  disk: { warning_pct_of_avg: 125, degraded_pct_of_avg: 150, critical_pct_of_avg: 200 },
  network: { warning_pct_of_avg: 125, degraded_pct_of_avg: 150, critical_pct_of_avg: 200 },
};

export function classifyPercent(value: number): HealthColor {
  if (!Number.isFinite(value)) return 'grey';
  if (value >= 90) return 'red';
  if (value >= 75) return 'amber';
  if (value >= 60) return 'yellow';
  return 'green';
}

export function worstColor(colors: HealthColor[]): HealthColor {
  return colors.reduce((worst, color) => (COLOR_RANK[color] > COLOR_RANK[worst] ? color : worst), 'green');
}

export function hostStatus(agent: Pick<DashboardAgent, 'maintenance' | 'components'>): HealthColor {
  if (agent.maintenance) return 'blue';
  const componentWorst = worstColor(agent.components.map((component) => component.color));
  return componentWorst === 'grey' ? 'yellow' : componentWorst;
}

export function formatBytes(bytes: number): string {
  if (!Number.isFinite(bytes) || bytes <= 0) return '0 B';
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let size = bytes;
  let unit = 0;
  while (size >= 1024 && unit < units.length - 1) {
    size /= 1024;
    unit += 1;
  }
  return `${size.toFixed(size >= 10 || unit === 0 ? 0 : 1)} ${units[unit]}`;
}

export function formatDuration(seconds: number): string {
  if (!Number.isFinite(seconds) || seconds <= 0) return 'unknown';
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  if (days > 0) return `${days}d ${hours}h`;
  const minutes = Math.floor((seconds % 3600) / 60);
  if (hours > 0) return `${hours}h ${minutes}m`;
  return `${minutes}m`;
}

function maxPercent(values: number[]): number {
  const finite = values.filter(Number.isFinite);
  return finite.length === 0 ? Number.NaN : Math.max(...finite);
}

function maintenanceComponents(): ComponentHealth[] {
  return ['cpu', 'memory', 'disk', 'network', 'temperature', 'processes', 'heartbeat'].map((key) => ({
    key: key as ComponentHealth['key'],
    label:
      key === 'temperature'
        ? 'Temp'
        : key === 'processes'
          ? 'Proc'
          : key === 'heartbeat'
            ? 'Heartbeat'
            : key[0].toUpperCase() + key.slice(1),
    color: 'blue' as HealthColor,
    value: 'maintenance',
    detail: 'Maintenance mode',
  }));
}

function heartbeatComponent(agent: AgentRecord): ComponentHealth {
  if (agent.maintenance) {
    return {
      key: 'heartbeat',
      label: 'Heartbeat',
      color: 'blue',
      value: 'maintenance',
      detail: 'Maintenance mode',
    };
  }
  return {
    key: 'heartbeat',
    label: 'Heartbeat',
    color: agent.connected ? 'green' : 'grey',
    value: agent.connected ? 'online' : 'offline',
    detail: agent.last_seen > 0 ? `Last seen ${new Date(agent.last_seen).toLocaleString()}` : 'No heartbeat seen',
  };
}

function noDataComponents(agent: AgentRecord): ComponentHealth[] {
  return [
    { key: 'cpu', label: 'CPU', color: 'grey', value: 'no data', detail: 'Awaiting metrics' },
    { key: 'memory', label: 'Memory', color: 'grey', value: 'no data', detail: 'Awaiting metrics' },
    { key: 'disk', label: 'Disk', color: 'grey', value: 'no data', detail: 'Awaiting metrics' },
    { key: 'network', label: 'Network', color: 'grey', value: 'no data', detail: 'Awaiting metrics' },
    { key: 'temperature', label: 'Temp', color: 'grey', value: 'no data', detail: 'Awaiting metrics' },
    { key: 'processes', label: 'Proc', color: 'grey', value: 'no data', detail: 'Awaiting metrics' },
    heartbeatComponent(agent),
  ];
}

function metricComponents(agent: AgentRecord, metrics?: SystemMetrics): ComponentHealth[] {
  if (agent.maintenance) return maintenanceComponents();
  if (!metrics) return noDataComponents(agent);

  const diskUsage = maxPercent(metrics.disks.map((disk) => disk.usage_percent));
  const maxTemp = maxPercent(metrics.temperatures.map((sensor) => sensor.temperature_celsius));
  const networkErrors = metrics.networks.reduce(
    (sum, net) => sum + net.errors_in + net.errors_out + net.drops_in + net.drops_out,
    0,
  );
  const networkDown = metrics.networks.some((net) => !net.is_up);
  const networkTraffic = metrics.networks.reduce((sum, net) => sum + net.bytes_recv_per_sec + net.bytes_sent_per_sec, 0);
  const topCpu = maxPercent(metrics.top_processes.map((process) => process.cpu_percent));

  return [
    {
      key: 'cpu',
      label: 'CPU',
      color: classifyPercent(metrics.cpu.usage_percent),
      value: `${metrics.cpu.usage_percent.toFixed(0)}%`,
      detail: `${metrics.cpu.num_logical_cores} logical cores`,
      percent: metrics.cpu.usage_percent,
    },
    {
      key: 'memory',
      label: 'Memory',
      color: classifyPercent(metrics.memory.usage_percent),
      value: `${metrics.memory.usage_percent.toFixed(0)}%`,
      detail: `${formatBytes(metrics.memory.used_bytes)} / ${formatBytes(metrics.memory.total_bytes)}`,
      percent: metrics.memory.usage_percent,
    },
    {
      key: 'disk',
      label: 'Disk',
      color: classifyPercent(diskUsage),
      value: metrics.disks.length === 0 ? 'none' : `${diskUsage.toFixed(0)}%`,
      detail: `${metrics.disks.length} mount${metrics.disks.length === 1 ? '' : 's'}`,
      percent: Number.isFinite(diskUsage) ? diskUsage : undefined,
    },
    {
      key: 'network',
      label: 'Network',
      color: networkErrors > 0 ? 'red' : networkDown ? 'yellow' : 'green',
      value: formatBytes(networkTraffic) + '/s',
      detail: networkErrors > 0 ? `${networkErrors} errors/drops` : `${metrics.networks.length} interface(s)`,
    },
    {
      key: 'temperature',
      label: 'Temp',
      color: metrics.temperatures.length === 0 ? 'grey' : classifyPercent(maxTemp),
      value: metrics.temperatures.length === 0 ? 'n/a' : `${maxTemp.toFixed(0)}C`,
      detail: `${metrics.temperatures.length} sensor(s)`,
      percent: metrics.temperatures.length === 0 ? undefined : Math.min(maxTemp, 100),
    },
    {
      key: 'processes',
      label: 'Proc',
      color: metrics.top_processes.length === 0 ? 'grey' : classifyPercent(topCpu),
      value: metrics.top_processes.length === 0 ? 'none' : `${topCpu.toFixed(0)}% top`,
      detail: `${metrics.top_processes.length} tracked process(es)`,
      percent: metrics.top_processes.length === 0 ? undefined : topCpu,
    },
    heartbeatComponent(agent),
  ];
}

export function toDashboardAgents(
  agents: AgentRecord[],
  snapshots: MetricsSnapshot[],
  alerts: AlertRecord[] = [],
): DashboardAgent[] {
  const latest = new Map(snapshots.map((snapshot) => [snapshot.agent_id, snapshot]));
  const unacked = new Set(alerts.filter((alert) => alert.acknowledged_at === 0).map((alert) => alert.agent_id));

  return agents
    .map((agent) => {
      const snapshot = latest.get(agent.agent_id);
      const metrics = snapshot?.metrics;
      const components = metricComponents(agent, metrics);
      const row: DashboardAgent = {
        id: agent.agent_id,
        name: metrics?.hostname || agent.hostname || agent.agent_id,
        platform: metrics?.platform || agent.platform || 'unknown',
        approved: agent.approved,
        rejected: agent.rejected,
        connected: agent.connected,
        maintenance: agent.maintenance,
        maintenanceReason: agent.maintenance_reason ?? '',
        maintenanceUntil: agent.maintenance_until ?? 0,
        collectionInterval: agent.collection_interval,
        processLimit: agent.process_limit,
        thresholds: agent.thresholds ?? DEFAULT_THRESHOLDS,
        lastSeen: agent.last_seen,
        uptime: metrics ? formatDuration(metrics.uptime_seconds) : 'unknown',
        group: agent.maintenance ? 'Maintenance Agents' : 'Approved Agents',
        groupIds: agent.group_ids ?? [],
        status: 'green',
        alertColor: unacked.has(agent.agent_id) ? 'red' : 'green',
        components,
        metrics,
      };
      row.status = hostStatus(row);
      return row;
    })
    .sort((a, b) => a.group.localeCompare(b.group) || a.name.localeCompare(b.name));
}

export function agentStatus(agent: DashboardAgent): HealthColor {
  return agent.status;
}

export function groupOverviewAgents(
  agents: DashboardAgent[],
  groups: GroupRecord[],
  filter: OverviewGroupFilter,
): OverviewAgentGroup[] {
  const groupNames = new Map(groups.map((group) => [group.group_id, group.name]));
  const buckets = new Map<string, OverviewAgentGroup>();

  const addToBucket = (id: string, name: string, agent: DashboardAgent) => {
    const bucket = buckets.get(id) ?? { id, name, agents: [] };
    bucket.agents.push(agent);
    buckets.set(id, bucket);
  };

  agents.forEach((agent) => {
    const groupIds = agent.groupIds.filter((id) => groupNames.has(id));
    if (filter === 'ungrouped') {
      if (groupIds.length === 0) addToBucket('ungrouped', 'Ungrouped', agent);
      return;
    }

    if (filter !== 'all') {
      const selected = Number.parseInt(filter, 10);
      if (Number.isFinite(selected) && groupIds.includes(selected)) {
        addToBucket(String(selected), groupNames.get(selected) ?? `Group ${selected}`, agent);
      }
      return;
    }

    if (groupIds.length === 0) {
      addToBucket('ungrouped', 'Ungrouped', agent);
      return;
    }

    groupIds.forEach((groupId) => {
      addToBucket(String(groupId), groupNames.get(groupId) ?? `Group ${groupId}`, agent);
    });
  });

  return [...buckets.values()]
    .map((group) => ({
      ...group,
      agents: [...group.agents].sort((a, b) => a.name.localeCompare(b.name)),
    }))
    .sort((a, b) => {
      if (a.id === 'ungrouped') return 1;
      if (b.id === 'ungrouped') return -1;
      return a.name.localeCompare(b.name);
    });
}

export function summaryCounts(agents: DashboardAgent[]): Record<SummaryKey, number> {
  const counts: Record<SummaryKey, number> = { green: 0, yellow: 0, amber: 0, red: 0, blue: 0, offline: 0 };
  agents.forEach((agent) => {
    if (!agent.connected && !agent.maintenance) {
      counts.offline += 1;
      return;
    }
    const status = agentStatus(agent);
    if (status === 'grey') counts.yellow += 1;
    else counts[status as SummaryKey] += 1;
  });
  return counts;
}

export function toDisplayAlerts(alerts: AlertRecord[], agents: DashboardAgent[]): DisplayAlert[] {
  const byId = new Map(agents.map((agent) => [agent.id, agent]));
  return alerts.map((alert) => {
    const agent = byId.get(alert.agent_id);
    return {
      ...alert,
      agentName: agent?.name ?? alert.agent_id,
      agentId: alert.agent_id,
    };
  });
}
