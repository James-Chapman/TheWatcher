import type { AgentRecord, MetricsSnapshot } from './models';

const API_BASE = import.meta.env.VITE_API_BASE_URL ?? '';

async function fetchJson<T>(path: string): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`);
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }
  return response.json() as Promise<T>;
}

async function mutateJson<T>(path: string, init: RequestInit): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`, init);
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }
  return response.json() as Promise<T>;
}

function jsonPost<T>(path: string, body: unknown): Promise<T> {
  return mutateJson<T>(path, {
    body: JSON.stringify(body),
    headers: { 'Content-Type': 'application/json' },
    method: 'POST',
  });
}

export async function fetchAgents(): Promise<AgentRecord[]> {
  return fetchJson<AgentRecord[]>('/api/agents');
}

export async function fetchLatestMetrics(): Promise<MetricsSnapshot[]> {
  return fetchJson<MetricsSnapshot[]>('/api/metrics');
}

export async function loadDashboardData(): Promise<{
  agents: AgentRecord[];
  metrics: MetricsSnapshot[];
}> {
  const [agents, metrics] = await Promise.all([fetchAgents(), fetchLatestMetrics()]);
  return { agents, metrics };
}

export async function approveAgent(agentId: string): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/approve`, { method: 'POST' });
}

export async function rejectAgent(agentId: string): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/reject`, { method: 'POST' });
}

export async function deleteAgent(agentId: string): Promise<void> {
  await mutateJson<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}`, { method: 'DELETE' });
}

export async function setAgentInterval(agentId: string, intervalSeconds: number): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/set_interval`, {
    interval_seconds: intervalSeconds,
  });
}

export async function setAgentProcessLimit(agentId: string, limit: number): Promise<void> {
  await jsonPost<{ ok: boolean }>(`/api/agents/${encodeURIComponent(agentId)}/set_process_limit`, { limit });
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
