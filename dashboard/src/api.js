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
export async function fetchAlerts(includeArchived = false) {
    return fetchJson(includeArchived ? '/api/alerts?include_archived=1' : '/api/alerts');
}
export async function fetchUnacknowledgedAlerts() {
    return fetchJson('/api/alerts/unacknowledged');
}
export async function loadDashboardData() {
    const [agents, pending, metrics, groups, alerts, allAlerts, users, maintenanceWindows, silences] = await Promise.all([
        fetchAgents(),
        fetchPendingEnrollments().catch(() => []),
        fetchLatestMetrics(),
        fetchGroups().catch(() => []),
        fetchUnacknowledgedAlerts().catch(() => []),
        fetchAlerts().catch(() => []),
        fetchUsers().catch(() => []),
        fetchMaintenanceWindows().catch(() => []),
        fetchSilences().catch(() => []),
    ]);
    return { agents, pending, metrics, groups, alerts, allAlerts, users, maintenanceWindows, silences };
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
export async function setAgentCollectorConfig(agentId, update) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/collector_config`, update);
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
export async function acknowledgeAlert(alertId, note = '') {
    await jsonPost(`/api/alerts/${alertId}/ack`, { note });
}
export async function bulkAcknowledgeAlerts(alertIds, note = '') {
    await jsonPost('/api/alerts/bulk-ack', { alert_ids: alertIds, note });
}
export async function deleteAlert(alertId) {
    await mutateJson(`/api/alerts/${alertId}`, { method: 'DELETE' });
}
export async function archiveAlert(alertId) {
    await mutateJson(`/api/alerts/${alertId}`, { method: 'DELETE' });
}
export async function bulkArchiveAlerts(alertIds) {
    await jsonPost('/api/alerts/bulk-archive', { alert_ids: alertIds });
}
export async function fetchMetricHistory(agentId, limit = 20) {
    return fetchJson(`/api/metrics/${encodeURIComponent(agentId)}?limit=${limit}`);
}
export async function fetchUptimeReport(agentId, days = 7) {
    return fetchJson(`/api/uptime/${encodeURIComponent(agentId)}?days=${days}`);
}
export async function fetchMaintenanceWindows() {
    return fetchJson('/api/maintenance-windows');
}
export async function createMaintenanceWindow(agentId, startMs, endMs, reason) {
    await jsonPost('/api/maintenance-windows', {
        agent_id: agentId,
        start_ms: startMs,
        end_ms: endMs,
        reason,
    });
}
export async function deleteMaintenanceWindow(windowId) {
    await mutateJson(`/api/maintenance-windows/${windowId}`, { method: 'DELETE' });
}
export async function setAgentDescription(agentId, description) {
    await jsonPost(`/api/agents/${encodeURIComponent(agentId)}/description`, { description });
}
export async function fetchSettings() {
    return fetchJson('/api/settings');
}
export async function updateSettings(settings) {
    await mutateJson('/api/settings', {
        body: JSON.stringify(settings),
        headers: { 'Content-Type': 'application/json' },
        method: 'PUT',
    });
}
export async function sendReport() {
    return mutateJson('/api/reports/send', { method: 'POST' });
}
export async function disableUser(userId) {
    await mutateJson(`/api/users/${userId}/disable`, { method: 'PUT' });
}
export async function enableUser(userId) {
    await mutateJson(`/api/users/${userId}/enable`, { method: 'PUT' });
}
export async function deleteUser(userId) {
    await mutateJson(`/api/users/${userId}`, { method: 'DELETE' });
}
export async function changeUserPassword(userId, password) {
    await mutateJson(`/api/users/${userId}/password`, {
        body: JSON.stringify({ password }),
        headers: { 'Content-Type': 'application/json' },
        method: 'PUT',
    });
}
export async function fetchSilences() {
    return fetchJson('/api/silences');
}
export async function createSilence(agentId, indicator, reason, untilMs) {
    await jsonPost('/api/silences', {
        agent_id: agentId,
        indicator,
        reason,
        until_ms: untilMs,
    });
}
export async function deleteSilence(silenceId) {
    await mutateJson(`/api/silences/${silenceId}`, { method: 'DELETE' });
}
export async function fetchAgentHistory(agentId, limit = 100) {
    return fetchJson(`/api/agents/${encodeURIComponent(agentId)}/history?limit=${limit}`);
}
export async function fetchLogMatches(agentId, limit = 200) {
    return fetchJson(`/api/agents/${encodeURIComponent(agentId)}/log-matches?limit=${limit}`);
}
export async function fetchViews() {
    return fetchJson('/api/views');
}
export async function createView(name, agentIds, isPublic) {
    return jsonPost('/api/views', { name, agent_ids: agentIds, is_public: isPublic });
}
export async function updateView(viewId, name, agentIds, isPublic) {
    return mutateJson(`/api/views/${viewId}`, {
        method: 'PUT',
        body: JSON.stringify({ name, agent_ids: agentIds, is_public: isPublic }),
        headers: { 'Content-Type': 'application/json' },
    });
}
export async function deleteView(viewId) {
    await mutateJson(`/api/views/${viewId}`, { method: 'DELETE' });
}
export async function fetchRunbooks() {
    return fetchJson('/api/runbooks');
}
export async function createRunbook(indicator, status, url, notes) {
    return jsonPost('/api/runbooks', { indicator, status, url, notes });
}
export async function deleteRunbook(runbookId) {
    await mutateJson(`/api/runbooks/${runbookId}`, { method: 'DELETE' });
}
