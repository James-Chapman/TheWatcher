import type {
  AgentCollectorConfigUpdate,
  AgentRecord,
  AgentThresholds,
  AlertRecord,
  GroupRecord,
  MetricsSnapshot,
  SessionInfo,
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

export async function fetchAlerts(): Promise<AlertRecord[]> {
  return fetchJson<AlertRecord[]>('/api/alerts');
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
  users: UserRecord[];
}> {
  const [agents, pending, metrics, groups, alerts, users] = await Promise.all([
    fetchAgents(),
    fetchPendingEnrollments().catch(() => []),
    fetchLatestMetrics(),
    fetchGroups().catch(() => []),
    fetchUnacknowledgedAlerts().catch(() => []),
    fetchUsers().catch(() => []),
  ]);
  return { agents, pending, metrics, groups, alerts, users };
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

export async function acknowledgeAlert(alertId: number): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/alerts/${alertId}/ack`, { method: 'POST' });
}

export async function deleteAlert(alertId: number): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/alerts/${alertId}`, { method: 'DELETE' });
}
