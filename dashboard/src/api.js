const API_BASE = import.meta.env.VITE_API_BASE_URL ?? '';
async function fetchJson(path) {
    const response = await fetch(`${API_BASE}${path}`);
    if (!response.ok) {
        throw new Error(`${response.status} ${response.statusText}`);
    }
    return response.json();
}
async function mutateJson(path, init) {
    const response = await fetch(`${API_BASE}${path}`, init);
    if (!response.ok) {
        throw new Error(`${response.status} ${response.statusText}`);
    }
    return response.json();
}
function jsonPost(path, body) {
    return mutateJson(path, {
        body: JSON.stringify(body),
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
    });
}
export async function fetchAgents() {
    return fetchJson('/api/agents');
}
export async function fetchLatestMetrics() {
    return fetchJson('/api/metrics');
}
export async function loadDashboardData() {
    const [agents, metrics] = await Promise.all([fetchAgents(), fetchLatestMetrics()]);
    return { agents, metrics };
}
export async function approveAgent(agentId) {
    await mutateJson(`/api/agents/${encodeURIComponent(agentId)}/approve`, { method: 'POST' });
}
export async function rejectAgent(agentId) {
    await mutateJson(`/api/agents/${encodeURIComponent(agentId)}/reject`, { method: 'POST' });
}
export async function deleteAgent(agentId) {
    await mutateJson(`/api/agents/${encodeURIComponent(agentId)}`, { method: 'DELETE' });
}
export async function setAgentInterval(agentId, intervalSeconds) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/set_interval`, {
        interval_seconds: intervalSeconds,
    });
}
export async function setAgentProcessLimit(agentId, limit) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/set_process_limit`, { limit });
}
export async function pauseAgent(agentId) {
    await mutateJson(`/api/agents/${encodeURIComponent(agentId)}/pause`, { method: 'POST' });
}
export async function resumeAgent(agentId) {
    await mutateJson(`/api/agents/${encodeURIComponent(agentId)}/resume`, { method: 'POST' });
}
export async function restartAgentCollectors(agentId) {
    await mutateJson(`/api/agents/${encodeURIComponent(agentId)}/restart_collectors`, { method: 'POST' });
}
export async function requestAgentStatus(agentId) {
    await mutateJson(`/api/agents/${encodeURIComponent(agentId)}/get_status`, { method: 'POST' });
}
