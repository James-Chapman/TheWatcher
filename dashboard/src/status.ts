import type {
  AgentRecord,
  ComponentHealth,
  DashboardAgent,
  HealthColor,
  MetricsSnapshot,
  SystemMetrics,
} from './models';

const COLOR_RANK: Record<HealthColor, number> = {
  green: 0,
  blue: 1,
  yellow: 2,
  orange: 3,
  red: 4,
};

export function classifyPercent(value: number): HealthColor {
  if (value >= 90) return 'red';
  if (value >= 75) return 'orange';
  if (value >= 60) return 'yellow';
  return 'green';
}

export function worstColor(colors: HealthColor[]): HealthColor {
  return colors.reduce((worst, color) => (COLOR_RANK[color] > COLOR_RANK[worst] ? color : worst), 'green');
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
  return values.length === 0 ? 0 : Math.max(...values.filter(Number.isFinite), 0);
}

function metricComponents(agent: AgentRecord, metrics?: SystemMetrics, now = Date.now()): ComponentHealth[] {
  const staleMinutes = agent.last_seen > 0 ? Math.floor((now - agent.last_seen) / 60000) : Number.POSITIVE_INFINITY;
  const heartbeatColor: HealthColor =
    staleMinutes === Number.POSITIVE_INFINITY ? 'red' : staleMinutes >= 10 ? 'red' : staleMinutes >= 3 ? 'yellow' : 'green';
  const maintenanceColor: HealthColor = agent.maintenance ? 'blue' : heartbeatColor;
  const maintenanceValue = agent.maintenance
    ? 'maintenance'
    : staleMinutes === Number.POSITIVE_INFINITY
      ? 'never'
      : `${staleMinutes}m`;
  const maintenanceDetail = agent.maintenance ? 'Maintenance mode' : 'Agent heartbeat age';

  if (!metrics) {
    return [
      { key: 'cpu', label: 'CPU', color: 'blue', value: 'no data', detail: 'Awaiting metrics' },
      { key: 'memory', label: 'Memory', color: 'blue', value: 'no data', detail: 'Awaiting metrics' },
      { key: 'disk', label: 'Disk', color: 'blue', value: 'no data', detail: 'Awaiting metrics' },
      { key: 'network', label: 'Network', color: 'blue', value: 'no data', detail: 'Awaiting metrics' },
      { key: 'temperature', label: 'Temp', color: 'blue', value: 'no data', detail: 'Awaiting metrics' },
      { key: 'processes', label: 'Proc', color: 'blue', value: 'no data', detail: 'Awaiting metrics' },
      {
        key: 'approval',
        label: 'Approval',
        color: agent.rejected ? 'red' : agent.approved ? 'green' : 'yellow',
        value: agent.rejected ? 'rejected' : agent.approved ? 'approved' : 'pending',
        detail: 'Enrollment state',
      },
      {
        key: 'heartbeat',
        label: 'Last Seen',
        color: maintenanceColor,
        value: maintenanceValue,
        detail: maintenanceDetail,
      },
    ];
  }

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
      percent: diskUsage,
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
      color: metrics.temperatures.length === 0 ? 'blue' : maxTemp >= 90 ? 'red' : maxTemp >= 75 ? 'orange' : maxTemp >= 65 ? 'yellow' : 'green',
      value: metrics.temperatures.length === 0 ? 'n/a' : `${maxTemp.toFixed(0)}C`,
      detail: `${metrics.temperatures.length} sensor(s)`,
      percent: metrics.temperatures.length === 0 ? undefined : Math.min(maxTemp, 100),
    },
    {
      key: 'processes',
      label: 'Proc',
      color: classifyPercent(topCpu),
      value: metrics.top_processes.length === 0 ? 'none' : `${topCpu.toFixed(0)}% top`,
      detail: `${metrics.top_processes.length} tracked process(es)`,
      percent: topCpu,
    },
    {
      key: 'approval',
      label: 'Approval',
      color: agent.rejected ? 'red' : agent.approved ? 'green' : 'yellow',
      value: agent.rejected ? 'rejected' : agent.approved ? 'approved' : 'pending',
      detail: 'Enrollment state',
    },
    {
      key: 'heartbeat',
      label: 'Last Seen',
      color: maintenanceColor,
      value: maintenanceValue,
      detail: maintenanceDetail,
    },
  ];
}

export function toDashboardAgents(
  agents: AgentRecord[],
  snapshots: MetricsSnapshot[],
  now = Date.now(),
): DashboardAgent[] {
  const latest = new Map(snapshots.map((snapshot) => [snapshot.agent_id, snapshot]));

  return agents
    .map((agent) => {
      const snapshot = latest.get(agent.agent_id);
      const metrics = snapshot?.metrics;
      return {
        id: agent.agent_id,
        name: metrics?.hostname || agent.hostname || agent.agent_id,
        platform: metrics?.platform || agent.platform || 'unknown',
        approved: agent.approved,
        rejected: agent.rejected,
        connected: agent.connected,
        maintenance: agent.maintenance,
        collectionInterval: agent.collection_interval,
        processLimit: agent.process_limit,
        lastSeen: agent.last_seen,
        uptime: metrics ? formatDuration(metrics.uptime_seconds) : 'unknown',
        group: agent.rejected
          ? 'Rejected Agents'
          : agent.maintenance
            ? 'Maintenance Agents'
            : agent.approved
              ? 'Approved Agents'
              : 'Pending Enrollment',
        components: metricComponents(agent, metrics, now),
        metrics,
      };
    })
    .sort((a, b) => a.group.localeCompare(b.group) || a.name.localeCompare(b.name));
}

export function agentStatus(agent: DashboardAgent): HealthColor {
  return worstColor(agent.components.map((component) => component.color));
}

export function summaryCounts(agents: DashboardAgent[]): Record<HealthColor, number> {
  const counts: Record<HealthColor, number> = { green: 0, yellow: 0, orange: 0, red: 0, blue: 0 };
  agents.forEach((agent) => {
    counts[agentStatus(agent)] += 1;
  });
  return counts;
}
