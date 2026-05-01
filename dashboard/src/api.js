const API_BASE = import.meta.env.VITE_API_BASE_URL ?? '';
async function fetchJson(path) {
    const response = await fetch(`${API_BASE}${path}`, { credentials: 'include' });
    if (!response.ok)
        throw new Error(`${response.status} ${response.statusText}`);
    return response.json();
}
async function mutateJson(path, init) {
    const response = await fetch(`${API_BASE}${path}`, { ...init, credentials: 'include' });
    if (!response.ok)
        throw new Error(`${response.status} ${response.statusText}`);
    return response.json();
}
function jsonPost(path, body) {
    return mutateJson(path, {
        body: JSON.stringify(body),
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
    });
}
export function fetchSession() {
    return fetchJson('/api/session');
}
export function login(username, password) {
    return jsonPost('/api/login', { username, password });
}
export async function logout() {
    await mutateJson('/api/logout', { method: 'POST' });
}
export async function fetchAgents() {
    return fetchJson('/api/agents');
}
export async function fetchPendingEnrollments() {
    return fetchJson('/api/pending-enrollments');
}
export async function fetchLatestMetrics() {
    return fetchJson('/api/metrics');
}
export async function fetchGroups() {
    return fetchJson('/api/groups');
}
export async function fetchUsers() {
    return fetchJson('/api/users');
}
export async function createGroup(name) {
    await jsonPost('/api/groups', { name });
}
export async function createUser(username, password, role, groupIds) {
    await jsonPost('/api/users', { username, password, role, group_ids: groupIds });
}
export async function fetchAlerts() {
    return fetchJson('/api/alerts');
}
export async function fetchUnacknowledgedAlerts() {
    return fetchJson('/api/alerts/unacknowledged');
}
export async function loadDashboardData() {
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
export async function approveAgent(agentId, groupIds = []) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/approve`, { group_ids: groupIds });
}
export async function rejectAgent(agentId) {
    await mutateJson(`/api/agents/${encodeURIComponent(agentId)}/reject`, { method: 'POST' });
}
export async function deleteAgent(agentId) {
    await mutateJson(`/api/agents/${encodeURIComponent(agentId)}`, { method: 'DELETE' });
}
export async function setAgentGroups(agentId, groupIds) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/groups`, { group_ids: groupIds });
}
export async function setAgentInterval(agentId, intervalSeconds) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/set_interval`, {
        interval_seconds: intervalSeconds,
    });
}
export async function setAgentProcessLimit(agentId, limit) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/set_process_limit`, { limit });
}
export async function setAgentThresholds(agentId, thresholds) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/thresholds`, { thresholds });
}
export async function setMaintenance(agentId, reason, untilMs) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/maintenance`, {
        reason,
        until_ms: untilMs,
    });
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
export async function acknowledgeAlert(alertId) {
    await mutateJson(`/api/alerts/${alertId}/ack`, { method: 'POST' });
}
export async function deleteAlert(alertId) {
    await mutateJson(`/api/alerts/${alertId}`, { method: 'DELETE' });
}
