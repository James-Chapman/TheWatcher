import type {
  AgentCollectorConfigUpdate,
  AgentRecord,
  AgentThresholds,
  AlertRecord,
  GroupRecord,
  MaintenanceWindowRecord,
  MetricsSnapshot,
  SessionInfo,
  UptimeReport,
  UserRecord,
} from './models';

const API_BASE = import.meta.env.VITE_API_BASE_URL ?? '';

async function fetchJson<T>(path: string): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`, { credentials: 'include' });
  if (!response.ok) throw new Error(`${response.status} ${response.statusText}`);
  return response.json() as Promise<T>;
}

async function mutateJson<T>(path: string, init: RequestInit): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`, { ...init, credentials: 'include' });
  if (!response.ok) throw new Error(`${response.status} ${response.statusText}`);
  return response.json() as Promise<T>;
}

function jsonPost<T>(path: string, body: unknown): Promise<T> {
  return mutateJson<T>(path, {
    body: JSON.stringify(body),
    headers: { 'Content-Type': 'application/json' },
    method: 'POST',
  });
}

export function fetchSession(): Promise<SessionInfo> {
  return fetchJson<SessionInfo>('/api/session');
}

export function login(username: string, password: string): Promise<SessionInfo> {
  return jsonPost<SessionInfo>('/api/login', { username, password });
}

export async function logout(): Promise<void> {
  await mutateJson<{ ok: boolean }>('/api/logout', { method: 'POST' });
}

export async function fetchAgents(): Promise<AgentRecord[]> {
  return fetchJson<AgentRecord[]>('/api/agents');
}

export async function fetchPendingEnrollments(): Promise<AgentRecord[]> {
  return fetchJson<AgentRecord[]>('/api/pending-enrollments');
}

export async function fetchLatestMetrics(): Promise<MetricsSnapshot[]> {
  return fetchJson<MetricsSnapshot[]>('/api/metrics');
}

export async function fetchGroups(): Promise<GroupRecord[]> {
  return fetchJson<GroupRecord[]>('/api/groups');
}

export async function fetchUsers(): Promise<UserRecord[]> {
  return fetchJson<UserRecord[]>('/api/users');
}

export async function createGroup(name: string): Promise<void> {
  await jsonPost<{ group_id: number }>('/api/groups', { name });
}

export async function createUser(username: string, password: string, role: UserRecord['role'], groupIds: number[]): Promise<void> {
  await jsonPost<{ user_id: number }>('/api/users', { username, password, role, group_ids: groupIds });
}

export async function fetchAlerts(includeArchived = false): Promise<AlertRecord[]> {
  return fetchJson<AlertRecord[]>(includeArchived ? '/api/alerts?include_archived=1' : '/api/alerts');
}

export async function fetchUnacknowledgedAlerts(): Promise<AlertRecord[]> {
  return fetchJson<AlertRecord[]>('/api/alerts/unacknowledged');
}

export async function loadDashboardData(): Promise<{
  agents: AgentRecord[];
  pending: AgentRecord[];
  metrics: MetricsSnapshot[];
  groups: GroupRecord[];
  alerts: AlertRecord[];
  allAlerts: AlertRecord[];
  users: UserRecord[];
  maintenanceWindows: MaintenanceWindowRecord[];
}> {
  const [agents, pending, metrics, groups, alerts, allAlerts, users, maintenanceWindows] = await Promise.all([
    fetchAgents(),
    fetchPendingEnrollments().catch(() => []),
    fetchLatestMetrics(),
    fetchGroups().catch(() => []),
    fetchUnacknowledgedAlerts().catch(() => []),
    fetchAlerts().catch(() => []),
    fetchUsers().catch(() => []),
    fetchMaintenanceWindows().catch(() => []),
  ]);
  return { agents, pending, metrics, groups, alerts, allAlerts, users, maintenanceWindows };
}

export async function approveAgent(agentId: string, groupIds: number[] = []): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/approve`, { group_ids: groupIds });
}

export async function rejectAgent(agentId: string): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/reject`, { method: 'POST' });
}

export async function deleteAgent(agentId: string): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}`, { method: 'DELETE' });
}

export async function setAgentGroups(agentId: string, groupIds: number[]): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/groups`, { group_ids: groupIds });
}

export async function setAgentInterval(agentId: string, intervalSeconds: number): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/set_interval`, {
    interval_seconds: intervalSeconds,
  });
}

export async function setAgentProcessLimit(agentId: string, limit: number): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/set_process_limit`, { limit });
}

export async function setAgentCollectorConfig(agentId: string, update: AgentCollectorConfigUpdate): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/collector_config`, update);
}

export async function setAgentThresholds(agentId: string, thresholds: AgentThresholds): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/thresholds`, { thresholds });
}

export async function setMaintenance(agentId: string, reason: string, untilMs: number): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/maintenance`, {
    reason,
    until_ms: untilMs,
  });
}

export async function pauseAgent(agentId: string): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/pause`, { method: 'POST' });
}

export async function resumeAgent(agentId: string): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/resume`, { method: 'POST' });
}

export async function restartAgentCollectors(agentId: string): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/restart_collectors`, { method: 'POST' });
}

export async function requestAgentStatus(agentId: string): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/get_status`, { method: 'POST' });
}

export async function acknowledgeAlert(alertId: number, note = ''): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/alerts/${alertId}/ack`, { note });
}

export async function bulkAcknowledgeAlerts(alertIds: number[], note = ''): Promise<void> {
  await jsonPost<{ ok: boolean }>('/api/alerts/bulk-ack', { alert_ids: alertIds, note });
}

export async function deleteAlert(alertId: number): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/alerts/${alertId}`, { method: 'DELETE' });
}

export async function archiveAlert(alertId: number): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/alerts/${alertId}`, { method: 'DELETE' });
}

export async function bulkArchiveAlerts(alertIds: number[]): Promise<void> {
  await jsonPost<{ ok: boolean }>('/api/alerts/bulk-archive', { alert_ids: alertIds });
}

export async function fetchMetricHistory(agentId: string, limit = 20): Promise<MetricsSnapshot[]> {
  return fetchJson<MetricsSnapshot[]>(`/api/metrics/${encodeURIComponent(agentId)}?limit=${limit}`);
}

export async function fetchUptimeReport(agentId: string, days = 7): Promise<UptimeReport> {
  return fetchJson<UptimeReport>(`/api/uptime/${encodeURIComponent(agentId)}?days=${days}`);
}

export async function fetchMaintenanceWindows(): Promise<MaintenanceWindowRecord[]> {
  return fetchJson<MaintenanceWindowRecord[]>('/api/maintenance-windows');
}

export async function createMaintenanceWindow(
  agentId: string,
  startMs: number,
  endMs: number,
  reason: string,
): Promise<void> {
  await jsonPost<{ window_id: number }>('/api/maintenance-windows', {
    agent_id: agentId,
    start_ms: startMs,
    end_ms: endMs,
    reason,
  });
}

export async function deleteMaintenanceWindow(windowId: number): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/maintenance-windows/${windowId}`, { method: 'DELETE' });
}
