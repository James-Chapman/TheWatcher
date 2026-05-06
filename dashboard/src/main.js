import { jsx as _jsx, jsxs as _jsxs, Fragment as _Fragment } from "react/jsx-runtime";
import React from 'react';
import { createRoot } from 'react-dom/client';
import { Archive, Check, CheckSquare, LogOut, Pause, Play, Plus, RefreshCw, RotateCw, Search, SlidersHorizontal, Square, Trash2, X } from 'lucide-react';
import { canManageAgent, maintenanceAction, maintenanceActionLabel } from './agentActions';
import { acknowledgeAlert, archiveAlert, approveAgent, bulkAcknowledgeAlerts, bulkArchiveAlerts, changeUserPassword, createGroup, createMaintenanceWindow, createSilence, createUser, deleteAgent, deleteMaintenanceWindow, deleteSilence, deleteUser, disableUser, enableUser, fetchAgentHistory, fetchAlerts, fetchMetricHistory, fetchSession, fetchSettings, fetchUptimeReport, loadDashboardData, login, logout, rejectAgent, requestAgentStatus, restartAgentCollectors, resumeAgent, setAgentCollectorConfig, setAgentDescription, setAgentGroups, setMaintenance, updateSettings, } from './api';
import { DEFAULT_ANOMALY_CONFIG, DEFAULT_NETWORK_THRESHOLDS, DEFAULT_PERCENT_THRESHOLDS, collectorConfigWithDefaults, groupOverviewAgents, summaryCounts, toDashboardAgents, toDisplayAlerts, } from './status';
import './styles.css';
const COMPONENT_LABELS = ['CPU', 'Memory', 'Disk', 'Network', 'Temp', 'Proc', 'Heartbeat'];
function colorLabel(color) {
    return {
        green: 'Healthy',
        yellow: 'Warning',
        amber: 'Degraded',
        red: 'Critical',
        grey: 'No Data',
        blue: 'Maintenance',
    }[color];
}
function Dashboard() {
    const [session, setSession] = React.useState(null);
    const [view, setView] = React.useState('monitoring');
    const [agents, setAgents] = React.useState([]);
    const [pending, setPending] = React.useState([]);
    const [groups, setGroups] = React.useState([]);
    const [alerts, setAlerts] = React.useState([]);
    const [allAlerts, setAllAlerts] = React.useState([]);
    const [users, setUsers] = React.useState([]);
    const [maintenanceWindows, setMaintenanceWindows] = React.useState([]);
    const [silences, setSilences] = React.useState([]);
    const [expanded, setExpanded] = React.useState(new Set());
    const [loadedAt, setLoadedAt] = React.useState(null);
    const [error, setError] = React.useState(null);
    const [loading, setLoading] = React.useState(true);
    const [busyAction, setBusyAction] = React.useState(null);
    const [groupDrafts, setGroupDrafts] = React.useState({});
    const [configAgentId, setConfigAgentId] = React.useState(null);
    const [overviewGroupFilter, setOverviewGroupFilter] = React.useState('all');
    const refresh = React.useCallback(async () => {
        if (!session)
            return;
        try {
            setError(null);
            const data = await loadDashboardData();
            setAgents(toDashboardAgents(data.agents, data.metrics, data.alerts));
            setPending(toDashboardAgents(data.pending, [], []));
            setGroups(data.groups);
            setAlerts(data.alerts);
            setAllAlerts(data.allAlerts);
            setUsers(data.users);
            setMaintenanceWindows(data.maintenanceWindows);
            setSilences(data.silences);
            setLoadedAt(new Date());
        }
        catch (err) {
            setError(err instanceof Error ? err.message : 'Failed to load dashboard data');
        }
        finally {
            setLoading(false);
        }
    }, [session]);
    React.useEffect(() => {
        void fetchSession()
            .then(setSession)
            .catch(() => setSession(null))
            .finally(() => setLoading(false));
    }, []);
    React.useEffect(() => {
        void refresh();
        const timer = window.setInterval(() => void refresh(), 5000);
        return () => window.clearInterval(timer);
    }, [refresh]);
    const runAction = React.useCallback(async (key, action) => {
        try {
            setBusyAction(key);
            setError(null);
            await action();
            await refresh();
        }
        catch (err) {
            setError(err instanceof Error ? err.message : 'Action failed');
        }
        finally {
            setBusyAction(null);
        }
    }, [refresh]);
    if (!session) {
        return _jsx(LoginScreen, { onLogin: setSession });
    }
    const counts = summaryCounts(agents);
    const displayAlerts = toDisplayAlerts(alerts, agents);
    const admin = session.role === 'admin';
    const operator = session.role === 'admin' || session.role === 'operator';
    const configAgent = agents.find((agent) => agent.id === configAgentId) ?? null;
    return (_jsxs(_Fragment, { children: [_jsxs("header", { className: "topbar", children: [_jsxs("div", { className: "logo", children: [_jsx("span", { className: "logo-dot" }), "THEWATCHER"] }), _jsxs("nav", { className: "nav-tabs", "aria-label": "Dashboard views", children: [_jsx(Tab, { view: view, target: "monitoring", setView: setView, label: "Monitoring" }), _jsx(Tab, { view: view, target: "agents", setView: setView, label: "Agents" }), admin ? _jsx(Tab, { view: view, target: "pending", setView: setView, label: "Pending" }) : null, _jsx(Tab, { view: view, target: "alerts", setView: setView, label: "Alerts" }), operator ? _jsx(Tab, { view: view, target: "maintenance", setView: setView, label: "Maintenance" }) : null, operator ? _jsx(Tab, { view: view, target: "silences", setView: setView, label: "Silences" }) : null, admin ? _jsx(Tab, { view: view, target: "users", setView: setView, label: "Users" }) : null, admin ? _jsx(Tab, { view: view, target: "settings", setView: setView, label: "Settings" }) : null] }), _jsxs("div", { className: "topbar-meta", children: [_jsx("span", { children: _jsx("strong", { children: loadedAt ? loadedAt.toUTCString().replace('GMT', 'UTC') : 'Not synced' }) }), _jsxs("span", { children: [session.username, " ", _jsx("strong", { children: session.role })] }), _jsx("button", { className: "icon-button", onClick: () => void refresh(), "aria-label": "Refresh dashboard", children: _jsx(RefreshCw, { size: 14 }) }), _jsx("button", { className: "icon-button", onClick: () => void logout().then(() => setSession(null)), "aria-label": "Log out", children: _jsx(LogOut, { size: 14 }) })] })] }), _jsxs("section", { className: "summary", children: [_jsx(SummaryCard, { label: "Healthy", value: counts.green, color: "green" }), _jsx(SummaryCard, { label: "Warning", value: counts.yellow, color: "yellow" }), _jsx(SummaryCard, { label: "Degraded", value: counts.amber, color: "amber" }), _jsx(SummaryCard, { label: "Critical", value: counts.red, color: "red" }), _jsx(SummaryCard, { label: "Maintenance", value: counts.blue, color: "blue" }), _jsx(SummaryCard, { label: "Offline", value: counts.offline, color: "grey" })] }), alerts.length > 0 && view === 'monitoring' ? (_jsx("div", { className: "alert-strip", children: displayAlerts.slice(0, 4).map((alert) => (_jsxs("div", { className: `alert-card ${alert.new_status}`, children: [_jsx(AgentIdentity, { id: alert.agentId, name: alert.agentName }), _jsxs("div", { className: "alert-card-detail", children: [_jsx("span", { children: alert.indicator }), _jsx("strong", { children: colorLabel(alert.new_status) })] }), _jsx("div", { className: "alert-card-message", children: alert.message })] }, alert.alert_id))) })) : null, view === 'monitoring' ? (_jsx(MonitoringTable, { agents: agents, error: error, expanded: expanded, groupFilter: overviewGroupFilter, groups: groups, loading: loading, setExpanded: setExpanded, setGroupFilter: setOverviewGroupFilter })) : null, view === 'agents' ? (_jsx(AgentManagement, { agents: agents, busyAction: busyAction, error: error, groupDrafts: groupDrafts, groups: groups, loading: loading, admin: admin, operator: operator, onConfigure: setConfigAgentId, runAction: runAction, setGroupDrafts: setGroupDrafts })) : null, configAgent ? (_jsx(AgentConfigModal, { agent: configAgent, busy: busyAction === `${configAgent.id}:collector_config`, onClose: () => setConfigAgentId(null), operator: operator && canManageAgent(configAgent), runAction: runAction })) : null, view === 'pending' ? (_jsx(PendingEnrollments, { agents: pending, busyAction: busyAction, groups: groups, runAction: runAction })) : null, view === 'alerts' ? (_jsx(AlertsPage, { agents: agents, alerts: toDisplayAlerts(allAlerts, agents), busyAction: busyAction, operator: operator, runAction: runAction })) : null, view === 'maintenance' ? (_jsx(MaintenanceWindowsPage, { agents: agents, busyAction: busyAction, groups: groups, operator: operator, runAction: runAction, windows: maintenanceWindows })) : null, view === 'silences' ? (_jsx(SilencesPage, { agents: agents, busyAction: busyAction, operator: operator, runAction: runAction, silences: silences })) : null, view === 'users' ? _jsx(UsersPage, { busyAction: busyAction, groups: groups, runAction: runAction, session: session, users: users }) : null, view === 'settings' ? _jsx(SettingsPage, { runAction: runAction }) : null] }));
}
function LoginScreen({ onLogin }) {
    const [username, setUsername] = React.useState('');
    const [password, setPassword] = React.useState('');
    const [error, setError] = React.useState(null);
    return (_jsx("main", { className: "login-wrap", children: _jsxs("form", { className: "login-panel", onSubmit: (event) => {
                event.preventDefault();
                void login(username, password).then(onLogin).catch((err) => setError(err instanceof Error ? err.message : 'Login failed'));
            }, children: [_jsxs("div", { className: "logo login-logo", children: [_jsx("span", { className: "logo-dot" }), "THEWATCHER"] }), error ? _jsxs("div", { className: "banner error", children: ["Login failed: ", error] }) : null, _jsxs("label", { children: ["Username", _jsx("input", { value: username, onChange: (event) => setUsername(event.target.value) })] }), _jsxs("label", { children: ["Password", _jsx("input", { type: "password", value: password, onChange: (event) => setPassword(event.target.value) })] }), _jsx("button", { className: "text-button", type: "submit", children: "Sign in" })] }) }));
}
function Tab({ view, target, setView, label }) {
    return (_jsx("button", { className: view === target ? 'active' : '', onClick: () => setView(target), children: label }));
}
function Sparkline({ data, width = 80, height = 24, color = 'var(--text)' }) {
    if (data.length < 2)
        return _jsx("svg", { width: width, height: height });
    const min = Math.min(...data);
    const max = Math.max(...data);
    const range = max - min || 1;
    const step = width / (data.length - 1);
    const points = data.map((v, i) => `${i * step},${height - ((v - min) / range) * (height - 2) - 1}`).join(' ');
    return (_jsx("svg", { width: width, height: height, style: { overflow: 'visible' }, children: _jsx("polyline", { points: points, fill: "none", stroke: color, strokeWidth: "1.5", strokeLinejoin: "round", strokeLinecap: "round" }) }));
}
function MonitoringTable({ agents, error, expanded, groupFilter, groups, loading, setExpanded, setGroupFilter, }) {
    const [agentHistory, setAgentHistory] = React.useState({});
    const [agentUptime, setAgentUptime] = React.useState({});
    const [agentStatusHistory, setAgentStatusHistory] = React.useState({});
    React.useEffect(() => {
        for (const id of expanded) {
            if (!agentHistory[id]) {
                void fetchMetricHistory(id, 20).then((snapshots) => {
                    setAgentHistory((prev) => ({ ...prev, [id]: snapshots }));
                }).catch(() => undefined);
                void fetchUptimeReport(id, 7).then((report) => {
                    setAgentUptime((prev) => ({ ...prev, [id]: report.uptime_percent }));
                }).catch(() => undefined);
                void fetchAgentHistory(id, 20).then((rows) => {
                    setAgentStatusHistory((prev) => ({ ...prev, [id]: rows }));
                }).catch(() => undefined);
            }
        }
    }, [expanded, agentHistory]);
    const overviewGroups = groupOverviewAgents(agents, groups, groupFilter);
    const visibleAgentCount = overviewGroups.reduce((count, group) => count + group.agents.length, 0);
    return (_jsxs("main", { className: "table-wrap", children: [error ? _jsxs("div", { className: "banner error", children: ["API error: ", error] }) : null, loading ? _jsx("div", { className: "banner", children: "Loading dashboard data..." }) : null, !loading && agents.length === 0 ? _jsx("div", { className: "banner", children: "No approved agents are reporting yet." }) : null, !loading && agents.length > 0 && visibleAgentCount === 0 ? _jsx("div", { className: "banner", children: "No agents match this group filter." }) : null, _jsx("div", { className: "overview-toolbar", children: _jsxs("label", { children: [_jsx(SlidersHorizontal, { size: 14 }), _jsx("span", { children: "Group" }), _jsxs("select", { value: groupFilter, onChange: (event) => setGroupFilter(event.target.value), children: [_jsx("option", { value: "all", children: "All groups" }), groups.map((group) => (_jsx("option", { value: String(group.group_id), children: group.name }, group.group_id))), _jsx("option", { value: "ungrouped", children: "Ungrouped" })] })] }) }), _jsx("div", { className: "table-container", children: _jsxs("table", { children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Server" }), _jsx("th", { children: "Uptime" }), _jsx("th", { children: "Status" }), _jsx("th", { children: "Alerts" }), COMPONENT_LABELS.map((label) => (_jsx("th", { children: label }, label)))] }) }), _jsx("tbody", { children: overviewGroups.map((group) => (_jsxs(React.Fragment, { children: [_jsx("tr", { className: "section-header", children: _jsxs("td", { colSpan: 4 + COMPONENT_LABELS.length, children: [group.name, " ", _jsx("span", { children: group.agents.length })] }) }), group.agents.map((agent) => (_jsxs(React.Fragment, { children: [_jsxs("tr", { className: expanded.has(agent.id) ? 'expanded' : '', onClick: () => {
                                                    setExpanded((current) => {
                                                        const next = new Set(current);
                                                        if (next.has(agent.id))
                                                            next.delete(agent.id);
                                                        else
                                                            next.add(agent.id);
                                                        return next;
                                                    });
                                                }, children: [_jsx("td", { children: _jsx(AgentIdentity, { id: agent.id, name: agent.name, prefix: _jsx("span", { className: "chevron", children: ">" }) }) }), _jsx("td", { className: "uptime", children: agent.uptime }), _jsx("td", { className: "dot-cell", children: _jsx(StatusDot, { color: agent.status, label: colorLabel(agent.status) }) }), _jsx("td", { className: "dot-cell", children: _jsx(StatusDot, { color: agent.alertColor, label: "Alerts" }) }), agent.components.map((component) => (_jsx("td", { className: "dot-cell", children: _jsxs("div", { className: "dot-wrap", children: [_jsx("span", { className: `dot ${component.color}` }), _jsxs("div", { className: "tooltip", children: [component.label, ": ", component.value, _jsx("br", {}), _jsx("span", { children: component.detail })] })] }) }, component.key)))] }), _jsx("tr", { className: `detail-row ${expanded.has(agent.id) ? '' : 'hidden'}`, children: _jsx("td", { colSpan: 4 + COMPONENT_LABELS.length, children: _jsxs("div", { className: "detail-inner", children: [agent.components.map((component) => {
                                                                const history = agentHistory[agent.id];
                                                                const sparkData = history
                                                                    ? component.key === 'cpu'
                                                                        ? history.map((s) => s.metrics.cpu.usage_percent).reverse()
                                                                        : component.key === 'memory'
                                                                            ? history.map((s) => s.metrics.memory.usage_percent).reverse()
                                                                            : []
                                                                    : [];
                                                                return (_jsxs("div", { className: "detail-card", children: [_jsxs("div", { className: "detail-card-header", children: [_jsx("span", { children: component.label }), _jsx("span", { className: `dot ${component.color} small-dot` })] }), _jsx("div", { className: `detail-card-value ${component.color}`, children: component.value }), _jsxs("div", { className: "detail-card-sub", children: [colorLabel(component.color), " / ", component.detail] }), sparkData.length >= 2 ? (_jsx("div", { className: "sparkline-wrap", children: _jsx(Sparkline, { data: sparkData, color: `var(--${component.color === 'green' ? 'green' : component.color === 'yellow' ? 'yellow' : component.color === 'amber' ? 'amber' : component.color === 'red' ? 'red' : 'muted'})` }) })) : null] }, component.key));
                                                            }), _jsxs("div", { className: "detail-card", children: [_jsx("div", { className: "detail-card-header", children: _jsx("span", { children: "Uptime" }) }), _jsx("div", { className: "detail-card-value green", children: agentUptime[agent.id] != null ? `${agentUptime[agent.id].toFixed(1)}%` : '—' }), _jsx("div", { className: "detail-card-sub", children: "Last 7 days" })] }), (agentStatusHistory[agent.id]?.length ?? 0) > 0 ? (_jsxs("div", { className: "detail-card detail-card-wide", children: [_jsx("div", { className: "detail-card-header", children: _jsx("span", { children: "Status History" }) }), _jsx("table", { className: "history-table", children: _jsx("tbody", { children: agentStatusHistory[agent.id].slice(0, 10).map((row) => (_jsxs("tr", { children: [_jsx("td", { className: "history-ts", children: new Date(row.created_at).toLocaleString() }), _jsx("td", { children: row.indicator }), _jsx("td", { children: _jsx("span", { className: `dot ${row.old_status} small-dot` }) }), _jsx("td", { children: "\u2192" }), _jsx("td", { children: _jsx("span", { className: `dot ${row.new_status} small-dot` }) }), _jsx("td", { className: "history-msg", children: row.message })] }, row.id))) }) })] })) : null] }) }) })] }, `${group.id}:${agent.id}`)))] }, group.id))) })] }) })] }));
}
function AgentManagement({ admin, agents, busyAction, error, groupDrafts, groups, loading, onConfigure, operator, runAction, setGroupDrafts, }) {
    const [descDrafts, setDescDrafts] = React.useState({});
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [error ? _jsxs("div", { className: "banner error", children: ["API error: ", error] }) : null, loading ? _jsx("div", { className: "banner", children: "Loading dashboard data..." }) : null, _jsx("div", { className: "management-header", children: _jsx("h1", { children: "Agent Management" }) }), _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Agent" }), _jsx("th", { children: "Platform" }), _jsx("th", { children: "State" }), _jsx("th", { children: "Last Seen" }), _jsx("th", { children: "Groups" }), _jsx("th", { children: "Commands" }), _jsx("th", { children: "Delete" })] }) }), _jsx("tbody", { children: agents.map((agent) => {
                                const groupValue = groupDrafts[agent.id] ?? String(agent.groupIds[0] ?? groups[0]?.group_id ?? 0);
                                const descValue = descDrafts[agent.id] ?? agent.description;
                                const manageable = operator && canManageAgent(agent);
                                const maintenanceCommand = maintenanceAction(agent);
                                return (_jsxs("tr", { children: [_jsx("td", { children: _jsxs("div", { className: "agent-config-cell", children: [_jsx(AgentIdentity, { id: agent.id, name: agent.name }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:configure`, disabled: !manageable, icon: _jsx(SlidersHorizontal, { size: 14 }), label: "Configure", onClick: () => onConfigure(agent.id) }), _jsxs("div", { className: "settings-grid desc-settings", children: [_jsx("input", { disabled: !operator, placeholder: "Description\u2026", value: descValue, onChange: (event) => setDescDrafts((current) => ({ ...current, [agent.id]: event.target.value })) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:desc`, disabled: !operator || descValue === agent.description, icon: _jsx(Check, { size: 14 }), label: "Set", onClick: () => runAction(`${agent.id}:desc`, () => setAgentDescription(agent.id, descValue)) })] })] }) }), _jsx("td", { children: agent.platform }), _jsx("td", { children: _jsx(StatusDot, { color: agent.status, label: colorLabel(agent.status) }) }), _jsx("td", { className: "uptime", children: agent.lastSeen > 0 ? `${Math.floor((Date.now() - agent.lastSeen) / 60000)}m` : 'never' }), _jsx("td", { children: _jsxs("div", { className: "settings-grid group-settings", children: [_jsxs("select", { disabled: !admin, value: groupValue, onChange: (event) => setGroupDrafts((current) => ({ ...current, [agent.id]: event.target.value })), children: [_jsx("option", { value: 0, children: "No group" }), groups.map((group) => (_jsx("option", { value: group.group_id, children: group.name }, group.group_id)))] }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:groups`, disabled: !admin, icon: _jsx(SlidersHorizontal, { size: 14 }), label: "Set", onClick: () => runAction(`${agent.id}:groups`, () => setAgentGroups(agent.id, Number(groupValue) > 0 ? [Number(groupValue)] : [])) })] }) }), _jsx("td", { children: _jsxs("div", { className: "button-row", children: [_jsx(ActionButton, { busy: busyAction === `${agent.id}:${maintenanceCommand}`, disabled: !manageable, icon: agent.maintenance ? _jsx(Play, { size: 14 }) : _jsx(Pause, { size: 14 }), label: maintenanceActionLabel(agent), onClick: () => runAction(`${agent.id}:${maintenanceCommand}`, () => agent.maintenance ? resumeAgent(agent.id) : setMaintenance(agent.id, 'operator request', 0)) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:restart`, disabled: !manageable, icon: _jsx(RotateCw, { size: 14 }), label: "Restart", onClick: () => runAction(`${agent.id}:restart`, () => restartAgentCollectors(agent.id)) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:status`, disabled: !manageable, icon: _jsx(Search, { size: 14 }), label: "Status", onClick: () => runAction(`${agent.id}:status`, () => requestAgentStatus(agent.id)) })] }) }), _jsx("td", { children: _jsx(ActionButton, { busy: busyAction === `${agent.id}:delete`, danger: true, disabled: !manageable, icon: _jsx(Trash2, { size: 14 }), label: "Delete", onClick: () => runAction(`${agent.id}:delete`, () => deleteAgent(agent.id)) }) })] }, agent.id));
                            }) })] }) })] }));
}
function buildCollectorDraft(agent) {
    const config = collectorConfigWithDefaults(agent.collectorConfig);
    const diskByMount = new Map(config.disks.map((disk) => [disk.mount_point, disk]));
    const metricDisks = agent.metrics?.disks
        .filter((disk) => !diskByMount.has(disk.mount_point))
        .map((disk) => ({
        mount_point: disk.mount_point,
        device: disk.device,
        enabled: true,
        thresholds: { ...DEFAULT_PERCENT_THRESHOLDS },
        anomaly: { ...DEFAULT_ANOMALY_CONFIG },
    })) ?? [];
    const networkByName = new Map(config.networks.map((network) => [network.interface_name, network]));
    const metricNetworks = agent.metrics?.networks
        .filter((network) => network.interface_name !== 'lo' && !networkByName.has(network.interface_name))
        .map((network) => ({
        interface_name: network.interface_name,
        enabled: true,
        thresholds: { ...DEFAULT_NETWORK_THRESHOLDS },
        anomaly: { ...DEFAULT_ANOMALY_CONFIG },
    })) ?? [];
    return {
        collection_interval: agent.collectionInterval,
        process_limit: agent.processLimit,
        collector_config: {
            ...config,
            cpu: { ...config.cpu },
            memory: { ...config.memory },
            disks: [
                ...config.disks.map((disk) => ({
                    ...disk,
                    thresholds: { ...disk.thresholds },
                    anomaly: { ...DEFAULT_ANOMALY_CONFIG, ...(disk.anomaly ?? {}) },
                })),
                ...metricDisks,
            ],
            networks: [
                ...config.networks.map((network) => ({
                    ...network,
                    thresholds: { ...network.thresholds },
                    anomaly: { ...DEFAULT_ANOMALY_CONFIG, ...(network.anomaly ?? {}) },
                })),
                ...metricNetworks,
            ],
            processes: config.processes.map((process) => ({ ...process })),
        },
    };
}
function AgentConfigModal({ agent, busy, onClose, operator, runAction, }) {
    const [draft, setDraft] = React.useState(() => buildCollectorDraft(agent));
    React.useEffect(() => {
        setDraft(buildCollectorDraft(agent));
    }, [agent]);
    const updateConfig = (update) => {
        setDraft((current) => ({ ...current, collector_config: update(current.collector_config) }));
    };
    const updatePercent = (collector, field, value) => {
        updateConfig((config) => ({
            ...config,
            [collector]: { ...config[collector], [field]: value },
        }));
    };
    const updateReading = (field, value) => {
        updateConfig((config) => ({ ...config, [field]: value }));
    };
    const updateDisk = (index, update) => {
        updateConfig((config) => ({
            ...config,
            disks: config.disks.map((disk, currentIndex) => (currentIndex === index ? update(disk) : disk)),
        }));
    };
    const updateNetwork = (index, update) => {
        updateConfig((config) => ({
            ...config,
            networks: config.networks.map((network, currentIndex) => (currentIndex === index ? update(network) : network)),
        }));
    };
    const updateProcess = (index, update) => {
        updateConfig((config) => ({
            ...config,
            processes: config.processes.map((process, currentIndex) => (currentIndex === index ? update(process) : process)),
        }));
    };
    return (_jsx("div", { className: "modal-backdrop", role: "presentation", children: _jsxs("form", { className: "config-modal", onSubmit: (event) => {
                event.preventDefault();
                void runAction(`${agent.id}:collector_config`, () => setAgentCollectorConfig(agent.id, draft));
            }, children: [_jsxs("div", { className: "modal-header", children: [_jsx(AgentIdentity, { id: agent.id, name: agent.name }), _jsx("button", { className: "icon-button", type: "button", onClick: onClose, "aria-label": "Close configuration", children: _jsx(X, { size: 14 }) })] }), _jsxs("div", { className: "config-grid", children: [_jsxs("section", { children: [_jsx("h2", { children: "Collection" }), _jsxs("div", { className: "form-grid", children: [_jsx(NumberField, { label: "Interval seconds", min: 1, value: draft.collection_interval, onChange: (value) => setDraft((current) => ({ ...current, collection_interval: value })) }), _jsx(NumberField, { label: "Process sample limit", min: 1, value: draft.process_limit, onChange: (value) => setDraft((current) => ({ ...current, process_limit: value })) }), _jsx(NumberField, { label: "CPU readings", min: 1, value: draft.collector_config.cpu_readings, onChange: (value) => updateReading('cpu_readings', value) }), _jsx(NumberField, { label: "Memory readings", min: 1, value: draft.collector_config.memory_readings, onChange: (value) => updateReading('memory_readings', value) }), _jsx(NumberField, { label: "Disk readings", min: 1, value: draft.collector_config.disk_readings, onChange: (value) => updateReading('disk_readings', value) }), _jsx(NumberField, { label: "Network readings", min: 1, value: draft.collector_config.network_readings, onChange: (value) => updateReading('network_readings', value) }), _jsx(NumberField, { label: "Process readings", min: 1, value: draft.collector_config.process_readings, onChange: (value) => updateReading('process_readings', value) })] })] }), _jsxs("section", { children: [_jsx("h2", { children: "CPU & Memory" }), _jsx(ThresholdRow, { label: "CPU %", thresholds: draft.collector_config.cpu, onChange: (field, value) => updatePercent('cpu', field, value) }), _jsx(AnomalyInputs, { anomaly: draft.collector_config.cpu_anomaly ?? DEFAULT_ANOMALY_CONFIG, onChange: (field, value) => updateConfig((config) => ({ ...config, cpu_anomaly: { ...(config.cpu_anomaly ?? DEFAULT_ANOMALY_CONFIG), [field]: value } })) }), _jsx(ThresholdRow, { label: "Memory %", thresholds: draft.collector_config.memory, onChange: (field, value) => updatePercent('memory', field, value) }), _jsx(AnomalyInputs, { anomaly: draft.collector_config.memory_anomaly ?? DEFAULT_ANOMALY_CONFIG, onChange: (field, value) => updateConfig((config) => ({ ...config, memory_anomaly: { ...(config.memory_anomaly ?? DEFAULT_ANOMALY_CONFIG), [field]: value } })) })] }), _jsxs("section", { className: "wide-section", children: [_jsx("h2", { children: "Fixed Disks" }), _jsxs("div", { className: "config-list", children: [draft.collector_config.disks.map((disk, index) => (_jsxs("div", { className: "config-row", children: [_jsxs("label", { className: "toggle-line", children: [_jsx("input", { type: "checkbox", checked: disk.enabled, onChange: (event) => updateDisk(index, (current) => ({ ...current, enabled: event.target.checked })) }), _jsx("span", { children: disk.device ? `${disk.mount_point} (${disk.device})` : disk.mount_point })] }), _jsx(ThresholdInputs, { thresholds: disk.thresholds, onChange: (field, value) => updateDisk(index, (current) => ({ ...current, thresholds: { ...current.thresholds, [field]: value } })) }), _jsx(AnomalyInputs, { anomaly: disk.anomaly ?? DEFAULT_ANOMALY_CONFIG, onChange: (field, value) => updateDisk(index, (current) => ({ ...current, anomaly: { ...(current.anomaly ?? DEFAULT_ANOMALY_CONFIG), [field]: value } })) })] }, disk.mount_point))), draft.collector_config.disks.length === 0 ? _jsx("div", { className: "empty-config", children: "No fixed disks reported yet." }) : null] })] }), _jsxs("section", { className: "wide-section", children: [_jsx("h2", { children: "Network Interfaces" }), _jsxs("div", { className: "config-list", children: [draft.collector_config.networks.map((network, index) => (_jsxs("div", { className: "config-row", children: [_jsxs("label", { className: "toggle-line", children: [_jsx("input", { type: "checkbox", checked: network.enabled, onChange: (event) => updateNetwork(index, (current) => ({ ...current, enabled: event.target.checked })) }), _jsx("span", { children: network.interface_name })] }), _jsx(NetworkThresholdInputs, { thresholds: network.thresholds, onChange: (field, value) => updateNetwork(index, (current) => ({ ...current, thresholds: { ...current.thresholds, [field]: value } })) }), _jsx(AnomalyInputs, { anomaly: network.anomaly ?? DEFAULT_ANOMALY_CONFIG, onChange: (field, value) => updateNetwork(index, (current) => ({ ...current, anomaly: { ...(current.anomaly ?? DEFAULT_ANOMALY_CONFIG), [field]: value } })) })] }, network.interface_name))), draft.collector_config.networks.length === 0 ? _jsx("div", { className: "empty-config", children: "No network interfaces reported yet." }) : null] })] }), _jsxs("section", { className: "wide-section", children: [_jsxs("div", { className: "section-title-row", children: [_jsx("h2", { children: "Process Watches" }), _jsx(ActionButton, { busy: false, icon: _jsx(Plus, { size: 14 }), label: "Process", onClick: () => updateConfig((config) => ({
                                                ...config,
                                                processes: [...config.processes, { name: '', expected_count: 1, enabled: true }],
                                            })) })] }), _jsxs("div", { className: "config-list", children: [draft.collector_config.processes.map((process, index) => (_jsxs("div", { className: "process-row", children: [_jsxs("label", { className: "toggle-line", children: [_jsx("input", { type: "checkbox", checked: process.enabled, onChange: (event) => updateProcess(index, (current) => ({ ...current, enabled: event.target.checked })) }), _jsx("span", { children: "Enabled" })] }), _jsx("input", { value: process.name, placeholder: "exact executable name", onChange: (event) => updateProcess(index, (current) => ({ ...current, name: event.target.value })) }), _jsx(NumberField, { label: "Expected", min: 1, value: process.expected_count, onChange: (value) => updateProcess(index, (current) => ({ ...current, expected_count: value })) }), _jsx(ActionButton, { busy: false, danger: true, icon: _jsx(Trash2, { size: 14 }), label: "Remove", onClick: () => updateConfig((config) => ({ ...config, processes: config.processes.filter((_, currentIndex) => currentIndex !== index) })) })] }, `${process.name}:${index}`))), draft.collector_config.processes.length === 0 ? _jsx("div", { className: "empty-config", children: "No process watches configured." }) : null] })] })] }), _jsxs("div", { className: "modal-actions", children: [_jsx("button", { className: "text-button", type: "button", onClick: onClose, children: "Cancel" }), _jsx("button", { className: "text-button", type: "submit", disabled: !operator || busy, children: busy ? '...' : 'Save' })] })] }) }));
}
function NumberField({ label, min, onChange, step = 1, value, }) {
    return (_jsxs("label", { children: [label, _jsx("input", { min: min, step: step, type: "number", value: value, onChange: (event) => onChange(Number(event.target.value)) })] }));
}
function AnomalyInputs({ anomaly, onChange, }) {
    return (_jsxs("div", { className: "anomaly-inputs", children: [_jsx(NumberField, { label: "Anomaly multiplier (0=off)", min: 0, step: 0.1, value: anomaly.multiplier, onChange: (value) => onChange('multiplier', value) }), _jsx(NumberField, { label: "Baseline hours", min: 1, value: anomaly.baseline_window_hours, onChange: (value) => onChange('baseline_window_hours', value) })] }));
}
function ThresholdInputs({ onChange, thresholds, }) {
    return (_jsxs("div", { className: "threshold-inputs", children: [_jsx(NumberField, { label: "Warn", min: 1, value: thresholds.warning_percent, onChange: (value) => onChange('warning_percent', value) }), _jsx(NumberField, { label: "Degrade", min: 1, value: thresholds.degraded_percent, onChange: (value) => onChange('degraded_percent', value) }), _jsx(NumberField, { label: "Critical", min: 1, value: thresholds.critical_percent, onChange: (value) => onChange('critical_percent', value) })] }));
}
function ThresholdRow({ label, onChange, thresholds, }) {
    return (_jsxs("div", { className: "threshold-row", children: [_jsx("span", { children: label }), _jsx(ThresholdInputs, { thresholds: thresholds, onChange: onChange })] }));
}
function NetworkThresholdInputs({ onChange, thresholds, }) {
    return (_jsxs("div", { className: "threshold-inputs", children: [_jsx(NumberField, { label: "Warn Mb/s", min: 1, value: thresholds.warning_mbps, onChange: (value) => onChange('warning_mbps', value) }), _jsx(NumberField, { label: "Degrade Mb/s", min: 1, value: thresholds.degraded_mbps, onChange: (value) => onChange('degraded_mbps', value) }), _jsx(NumberField, { label: "Critical Mb/s", min: 1, value: thresholds.critical_mbps, onChange: (value) => onChange('critical_mbps', value) })] }));
}
function PendingEnrollments({ agents, busyAction, groups, runAction }) {
    const defaultGroup = groups[0]?.group_id;
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [_jsx("div", { className: "management-header", children: _jsx("h1", { children: "Pending Enrollments" }) }), _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Agent" }), _jsx("th", { children: "Platform" }), _jsx("th", { children: "Last Seen" }), _jsx("th", { children: "Actions" })] }) }), _jsx("tbody", { children: agents.map((agent) => (_jsxs("tr", { children: [_jsx("td", { children: _jsx(AgentIdentity, { id: agent.id, name: agent.name }) }), _jsx("td", { children: agent.platform }), _jsx("td", { className: "uptime", children: agent.lastSeen > 0 ? `${Math.floor((Date.now() - agent.lastSeen) / 60000)}m` : 'never' }), _jsx("td", { children: _jsxs("div", { className: "button-row", children: [_jsx(ActionButton, { busy: busyAction === `${agent.id}:approve`, icon: _jsx(Check, { size: 14 }), label: "Approve", onClick: () => runAction(`${agent.id}:approve`, () => approveAgent(agent.id, defaultGroup ? [defaultGroup] : [])) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:reject`, icon: _jsx(X, { size: 14 }), label: "Reject", onClick: () => runAction(`${agent.id}:reject`, () => rejectAgent(agent.id)) })] }) })] }, agent.id))) })] }) })] }));
}
function AcknowledgeModal({ onClose, onSubmit, bulk }) {
    const [note, setNote] = React.useState('');
    const [andArchive, setAndArchive] = React.useState(false);
    return (_jsx("div", { className: "modal-backdrop", onClick: onClose, children: _jsxs("div", { className: "modal-panel", onClick: (e) => e.stopPropagation(), children: [_jsxs("div", { className: "modal-header", children: [_jsx("h2", { children: bulk ? 'Acknowledge selected alerts' : 'Acknowledge alert' }), _jsx("button", { className: "icon-button", onClick: onClose, children: _jsx(X, { size: 14 }) })] }), _jsxs("label", { children: ["Note ", _jsx("span", { className: "field-hint", children: "(optional)" }), _jsx("textarea", { className: "note-input", rows: 3, placeholder: "Describe what you found or what action was taken\u2026", value: note, onChange: (e) => setNote(e.target.value), autoFocus: true })] }), _jsxs("label", { className: "checkbox-label", children: [_jsx("input", { type: "checkbox", checked: andArchive, onChange: (e) => setAndArchive(e.target.checked) }), "Also archive after acknowledging"] }), _jsxs("div", { className: "button-row modal-actions", children: [_jsx("button", { className: "text-button secondary", onClick: onClose, children: "Cancel" }), _jsx("button", { className: "text-button", onClick: () => onSubmit(note, andArchive), children: andArchive ? 'Acknowledge & Archive' : 'Acknowledge' })] })] }) }));
}
function AlertsPage({ agents, alerts, busyAction, operator, runAction, }) {
    const [selected, setSelected] = React.useState(new Set());
    const [showArchived, setShowArchived] = React.useState(false);
    const [archivedAlerts, setArchivedAlerts] = React.useState([]);
    const [agentFilter, setAgentFilter] = React.useState('');
    const [statusFilter, setStatusFilter] = React.useState('');
    const [ackTarget, setAckTarget] = React.useState(null);
    // Re-fetch archived alerts whenever the live alerts list refreshes (proxy for parent refresh)
    React.useEffect(() => {
        if (!showArchived)
            return;
        void fetchAlerts(true).then((all) => setArchivedAlerts(toDisplayAlerts(all, agents)));
    }, [showArchived, alerts, agents]);
    const baseAlerts = showArchived ? archivedAlerts : alerts;
    const displayed = baseAlerts.filter((a) => {
        if (agentFilter && a.agentId !== agentFilter)
            return false;
        if (statusFilter && a.new_status !== statusFilter)
            return false;
        return true;
    });
    const displayedIds = displayed.map((a) => a.alert_id);
    const allSelected = displayedIds.length > 0 && displayedIds.every((id) => selected.has(id));
    const toggleSelect = (id) => setSelected((prev) => { const n = new Set(prev); n.has(id) ? n.delete(id) : n.add(id); return n; });
    const toggleSelectAll = () => setSelected(allSelected ? new Set() : new Set(displayedIds));
    const afterAction = () => setSelected(new Set());
    const handleAckSubmit = async (note, andArchive) => {
        setAckTarget(null);
        if (ackTarget === 'bulk') {
            const ids = [...selected];
            await runAction('bulk:ack', async () => {
                await bulkAcknowledgeAlerts(ids, note);
                if (andArchive)
                    await bulkArchiveAlerts(ids);
            });
        }
        else if (ackTarget !== null) {
            const id = ackTarget;
            await runAction(`alert:${id}:ack`, async () => {
                await acknowledgeAlert(id, note);
                if (andArchive)
                    await archiveAlert(id);
            });
        }
        afterAction();
    };
    const uniqueAgents = [...new Map(alerts.map((a) => [a.agentId, a.agentName])).entries()];
    const selectedCount = selected.size;
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [ackTarget !== null ? (_jsx(AcknowledgeModal, { bulk: ackTarget === 'bulk', onClose: () => setAckTarget(null), onSubmit: handleAckSubmit })) : null, _jsxs("div", { className: "management-header", children: [_jsx("h1", { children: "Alerts" }), _jsxs("div", { className: "header-controls", children: [_jsxs("select", { value: agentFilter, onChange: (e) => setAgentFilter(e.target.value), children: [_jsx("option", { value: "", children: "All agents" }), uniqueAgents.map(([id, name]) => _jsx("option", { value: id, children: name }, id))] }), _jsxs("select", { value: statusFilter, onChange: (e) => setStatusFilter(e.target.value), children: [_jsx("option", { value: "", children: "All statuses" }), _jsx("option", { value: "red", children: "Critical" }), _jsx("option", { value: "amber", children: "Degraded" }), _jsx("option", { value: "yellow", children: "Warning" }), _jsx("option", { value: "green", children: "Healthy" })] }), _jsxs("label", { className: "checkbox-label", children: [_jsx("input", { type: "checkbox", checked: showArchived, onChange: (e) => setShowArchived(e.target.checked) }), "Show archived"] })] })] }), selectedCount > 0 && operator ? (_jsxs("div", { className: "bulk-action-bar", children: [_jsxs("span", { children: [selectedCount, " selected"] }), _jsxs("div", { className: "button-row", children: [_jsx(ActionButton, { busy: busyAction === 'bulk:ack', icon: _jsx(Check, { size: 14 }), label: `Acknowledge (${selectedCount})`, onClick: () => setAckTarget('bulk') }), _jsx(ActionButton, { busy: busyAction === 'bulk:archive', danger: true, icon: _jsx(Archive, { size: 14 }), label: `Archive (${selectedCount})`, onClick: () => runAction('bulk:archive', async () => { await bulkArchiveAlerts([...selected]); afterAction(); }) })] })] })) : null, _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { className: "col-check", children: _jsx("button", { className: "icon-button", onClick: toggleSelectAll, disabled: displayedIds.length === 0, children: allSelected ? _jsx(CheckSquare, { size: 14 }) : _jsx(Square, { size: 14 }) }) }), _jsx("th", { children: "Agent" }), _jsx("th", { children: "Indicator" }), _jsx("th", { children: "State" }), _jsx("th", { children: "Created" }), _jsx("th", { children: "Message" }), _jsx("th", { children: "Acknowledged" }), _jsx("th", { children: "Actions" })] }) }), _jsx("tbody", { children: displayed.map((alert) => {
                                const isAcknowledged = alert.acknowledged_at > 0;
                                const isArchived = alert.deleted_at > 0;
                                return (_jsxs("tr", { className: isArchived ? 'row-archived' : '', children: [_jsx("td", { className: "col-check", children: _jsx("button", { className: "icon-button", onClick: () => toggleSelect(alert.alert_id), children: selected.has(alert.alert_id) ? _jsx(CheckSquare, { size: 14 }) : _jsx(Square, { size: 14 }) }) }), _jsx("td", { children: _jsx(AgentIdentity, { id: alert.agentId, name: alert.agentName }) }), _jsx("td", { children: alert.indicator }), _jsx("td", { children: _jsx(StatusDot, { color: alert.new_status, label: colorLabel(alert.new_status) }) }), _jsx("td", { className: "uptime", children: new Date(alert.created_at).toLocaleString() }), _jsx("td", { children: alert.message }), _jsx("td", { className: "uptime", children: isAcknowledged ? (_jsxs("span", { children: [alert.acknowledged_by, " \u2014 ", new Date(alert.acknowledged_at).toLocaleString(), alert.note ? _jsxs("em", { className: "alert-note", children: [" \u2014 ", alert.note] }) : null] })) : alert.escalated_at > 0 ? (_jsx("span", { className: "escalated-badge", children: "ESCALATED" })) : null }), _jsx("td", { children: !isArchived ? (_jsx("div", { className: "button-row", children: !isAcknowledged ? (_jsxs(_Fragment, { children: [_jsx(ActionButton, { busy: busyAction === `alert:${alert.alert_id}:ack`, disabled: !operator, icon: _jsx(Check, { size: 14 }), label: "Acknowledge", onClick: () => setAckTarget(alert.alert_id) }), _jsx(ActionButton, { busy: busyAction === `alert:${alert.alert_id}:ack-archive`, disabled: !operator, icon: _jsx(Archive, { size: 14 }), label: "Acknowledge & Archive", onClick: () => runAction(`alert:${alert.alert_id}:ack-archive`, async () => { await acknowledgeAlert(alert.alert_id); await archiveAlert(alert.alert_id); }) })] })) : (_jsx(ActionButton, { busy: busyAction === `alert:${alert.alert_id}:archive`, danger: true, disabled: !operator, icon: _jsx(Archive, { size: 14 }), label: "Archive", onClick: () => runAction(`alert:${alert.alert_id}:archive`, () => archiveAlert(alert.alert_id)) })) })) : _jsx("span", { className: "uptime", children: "Archived" }) })] }, alert.alert_id));
                            }) })] }) })] }));
}
function MaintenanceWindowsPage({ agents, busyAction, groups, operator, runAction, windows, }) {
    const [agentId, setAgentId] = React.useState('*');
    const [reason, setReason] = React.useState('');
    const [startDate, setStartDate] = React.useState('');
    const [startTime, setStartTime] = React.useState('00:00');
    const [durationHours, setDurationHours] = React.useState(1);
    const fmt = (ms) => new Date(ms).toLocaleString();
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [_jsx("div", { className: "management-header", children: _jsx("h1", { children: "Maintenance Windows" }) }), operator ? (_jsxs("form", { className: "inline-form maint-form", onSubmit: (event) => {
                    event.preventDefault();
                    if (!startDate)
                        return;
                    const startMs = new Date(`${startDate}T${startTime}`).getTime();
                    const endMs = startMs + durationHours * 3600_000;
                    if (Number.isNaN(startMs) || endMs <= startMs)
                        return;
                    void runAction('mw:create', () => createMaintenanceWindow(agentId, startMs, endMs, reason)).then(() => {
                        setReason('');
                    });
                }, children: [_jsxs("select", { "aria-label": "Agent", value: agentId, onChange: (event) => setAgentId(event.target.value), children: [_jsx("option", { value: "*", children: "All agents" }), agents.map((a) => _jsx("option", { value: a.id, children: a.name || a.id }, a.id))] }), _jsx("input", { "aria-label": "Reason", placeholder: "Reason", value: reason, onChange: (event) => setReason(event.target.value) }), _jsx("input", { "aria-label": "Start date", type: "date", value: startDate, onChange: (event) => setStartDate(event.target.value) }), _jsx("input", { "aria-label": "Start time", type: "time", value: startTime, onChange: (event) => setStartTime(event.target.value) }), _jsxs("label", { children: ["Duration (h)", _jsx("input", { "aria-label": "Duration hours", min: 1, step: 1, type: "number", value: durationHours, onChange: (event) => setDurationHours(Math.max(1, Number(event.target.value))), style: { width: 60 } })] }), _jsx(ActionButton, { busy: busyAction === 'mw:create', disabled: !operator, icon: _jsx(Plus, { size: 14 }), label: "Schedule", onClick: () => undefined, type: "submit" })] })) : null, _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Agent" }), _jsx("th", { children: "Start" }), _jsx("th", { children: "End" }), _jsx("th", { children: "Reason" }), _jsx("th", { children: "Created by" }), _jsx("th", { children: "Delete" })] }) }), _jsxs("tbody", { children: [windows.length === 0 ? (_jsx("tr", { children: _jsx("td", { colSpan: 6, className: "uptime", children: "No maintenance windows scheduled." }) })) : null, windows.map((w) => {
                                    const agent = agents.find((a) => a.id === w.agent_id);
                                    const now = Date.now();
                                    const active = w.start_ms <= now && w.end_ms > now;
                                    return (_jsxs("tr", { className: active ? 'maint-active' : '', children: [_jsx("td", { children: w.agent_id === '*' ? _jsx("em", { children: "All agents" }) : _jsx(AgentIdentity, { id: w.agent_id, name: agent?.name ?? w.agent_id }) }), _jsx("td", { className: "uptime", children: fmt(w.start_ms) }), _jsx("td", { className: "uptime", children: fmt(w.end_ms) }), _jsx("td", { children: w.reason }), _jsx("td", { children: w.created_by }), _jsx("td", { children: _jsx(ActionButton, { busy: busyAction === `mw:delete:${w.window_id}`, danger: true, disabled: !operator, icon: _jsx(Trash2, { size: 14 }), label: "Delete", onClick: () => runAction(`mw:delete:${w.window_id}`, () => deleteMaintenanceWindow(w.window_id)) }) })] }, w.window_id));
                                })] })] }) })] }));
}
function SilencesPage({ agents, busyAction, operator, runAction, silences, }) {
    const [agentId, setAgentId] = React.useState('*');
    const [indicator, setIndicator] = React.useState('*');
    const [reason, setReason] = React.useState('');
    const [hours, setHours] = React.useState(1);
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [_jsx("div", { className: "management-header", children: _jsx("h1", { children: "Silence Rules" }) }), operator ? (_jsxs("form", { className: "settings-form", onSubmit: (event) => {
                    event.preventDefault();
                    const untilMs = Date.now() + hours * 3_600_000;
                    void runAction('silence:create', () => createSilence(agentId, indicator, reason, untilMs)).then(() => {
                        setReason('');
                    });
                }, children: [_jsxs("div", { className: "form-grid", children: [_jsxs("label", { children: ["Agent", _jsxs("select", { value: agentId, onChange: (e) => setAgentId(e.target.value), children: [_jsx("option", { value: "*", children: "All agents" }), agents.map((a) => (_jsx("option", { value: a.id, children: a.name || a.id }, a.id)))] })] }), _jsxs("label", { children: ["Indicator", _jsxs("select", { value: indicator, onChange: (e) => setIndicator(e.target.value), children: [_jsx("option", { value: "*", children: "All indicators" }), _jsx("option", { value: "cpu", children: "cpu" }), _jsx("option", { value: "memory", children: "memory" }), _jsx("option", { value: "temperature", children: "temperature" }), _jsx("option", { value: "heartbeat", children: "heartbeat" })] })] }), _jsxs("label", { children: ["Duration (hours)", _jsx("input", { min: 1, max: 168, step: 1, type: "number", value: hours, onChange: (e) => setHours(Number(e.target.value)) })] }), _jsxs("label", { children: ["Reason", _jsx("input", { placeholder: "Planned maintenance\u2026", value: reason, onChange: (e) => setReason(e.target.value) })] })] }), _jsx("div", { className: "button-row", children: _jsxs("button", { className: "text-button", disabled: busyAction !== null, type: "submit", children: [_jsx(Plus, { size: 14 }), " Add Silence Rule"] }) })] })) : null, _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Agent" }), _jsx("th", { children: "Indicator" }), _jsx("th", { children: "Reason" }), _jsx("th", { children: "Expires" }), _jsx("th", { children: "Created by" }), operator ? _jsx("th", { children: "Delete" }) : null] }) }), _jsxs("tbody", { children: [silences.length === 0 ? (_jsx("tr", { children: _jsx("td", { colSpan: operator ? 6 : 5, className: "uptime", children: "No active silence rules." }) })) : null, silences.map((s) => {
                                    const agent = agents.find((a) => a.id === s.agent_id);
                                    const expired = s.until_ms <= Date.now();
                                    return (_jsxs("tr", { className: expired ? 'row-muted' : '', children: [_jsx("td", { children: s.agent_id === '*' ? 'All agents' : (agent?.name || s.agent_id) }), _jsx("td", { children: s.indicator === '*' ? 'All indicators' : s.indicator }), _jsx("td", { children: s.reason }), _jsxs("td", { children: [new Date(s.until_ms).toLocaleString(), expired ? ' (expired)' : ''] }), _jsx("td", { children: s.created_by }), operator ? (_jsx("td", { children: _jsx(ActionButton, { busy: busyAction === `silence:delete:${s.silence_id}`, danger: true, icon: _jsx(Trash2, { size: 14 }), label: "Delete", onClick: () => runAction(`silence:delete:${s.silence_id}`, () => deleteSilence(s.silence_id)) }) })) : null] }, s.silence_id));
                                })] })] }) })] }));
}
function UsersPage({ busyAction, groups, runAction, session, users, }) {
    const [groupName, setGroupName] = React.useState('');
    const [username, setUsername] = React.useState('');
    const [password, setPassword] = React.useState('');
    const [role, setRole] = React.useState('viewer');
    const [groupId, setGroupId] = React.useState(groups[0]?.group_id ?? 0);
    const [pwUserId, setPwUserId] = React.useState(null);
    const [pwValue, setPwValue] = React.useState('');
    const admin = session.role === 'admin';
    const currentUser = users.find((u) => u.username === session.username);
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [pwUserId !== null ? (_jsx("div", { className: "modal-backdrop", onClick: () => setPwUserId(null), children: _jsxs("div", { className: "modal-panel", onClick: (e) => e.stopPropagation(), children: [_jsxs("div", { className: "modal-header", children: [_jsx("h2", { children: "Change Password" }), _jsx("button", { className: "icon-button", onClick: () => setPwUserId(null), children: _jsx(X, { size: 14 }) })] }), _jsxs("label", { children: ["New password", _jsx("input", { type: "password", autoFocus: true, value: pwValue, onChange: (e) => setPwValue(e.target.value) })] }), _jsxs("div", { className: "button-row modal-actions", children: [_jsx("button", { className: "text-button secondary", onClick: () => setPwUserId(null), children: "Cancel" }), _jsx("button", { className: "text-button", disabled: !pwValue, onClick: () => {
                                        const id = pwUserId;
                                        const pw = pwValue;
                                        setPwUserId(null);
                                        setPwValue('');
                                        void runAction(`user:${id}:pw`, () => changeUserPassword(id, pw));
                                    }, children: "Save" })] })] }) })) : null, _jsx("div", { className: "management-header", children: _jsx("h1", { children: "Users & Groups" }) }), _jsxs("div", { className: "management-grid", children: [_jsxs("form", { className: "inline-form", onSubmit: (event) => {
                            event.preventDefault();
                            if (!groupName.trim())
                                return;
                            void runAction('group:create', () => createGroup(groupName.trim())).then(() => setGroupName(''));
                        }, children: [_jsx("input", { "aria-label": "Group name", placeholder: "Group name", value: groupName, onChange: (event) => setGroupName(event.target.value) }), _jsx(ActionButton, { busy: busyAction === 'group:create', icon: _jsx(Plus, { size: 14 }), label: "Group", onClick: () => undefined, type: "submit" })] }), _jsxs("form", { className: "inline-form user-form", onSubmit: (event) => {
                            event.preventDefault();
                            if (!username.trim() || !password)
                                return;
                            void runAction('user:create', () => createUser(username.trim(), password, role, groupId ? [groupId] : [])).then(() => {
                                setUsername('');
                                setPassword('');
                            });
                        }, children: [_jsx("input", { "aria-label": "Username", placeholder: "Username", value: username, onChange: (event) => setUsername(event.target.value) }), _jsx("input", { "aria-label": "Password", placeholder: "Password", type: "password", value: password, onChange: (event) => setPassword(event.target.value) }), _jsxs("select", { "aria-label": "Role", value: role, onChange: (event) => setRole(event.target.value), children: [_jsx("option", { value: "viewer", children: "Viewer" }), _jsx("option", { value: "operator", children: "Operator" }), _jsx("option", { value: "admin", children: "Admin" })] }), _jsxs("select", { "aria-label": "Group", value: groupId, onChange: (event) => setGroupId(Number(event.target.value)), children: [_jsx("option", { value: 0, children: "No group" }), groups.map((group) => (_jsx("option", { value: group.group_id, children: group.name }, group.group_id)))] }), _jsx(ActionButton, { busy: busyAction === 'user:create', icon: _jsx(Plus, { size: 14 }), label: "User", onClick: () => undefined, type: "submit" })] })] }), _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "User" }), _jsx("th", { children: "Role" }), _jsx("th", { children: "Groups" }), _jsx("th", { children: "State" }), _jsx("th", { children: "Actions" })] }) }), _jsx("tbody", { children: users.map((user) => {
                                const canChangePw = admin || currentUser?.user_id === user.user_id;
                                return (_jsxs("tr", { className: user.disabled ? 'row-archived' : '', children: [_jsx("td", { children: user.username }), _jsx("td", { children: user.role }), _jsx("td", { children: user.group_ids.map((id) => groups.find((group) => group.group_id === id)?.name ?? id).join(', ') }), _jsx("td", { children: user.disabled ? 'Disabled' : 'Active' }), _jsx("td", { children: _jsxs("div", { className: "button-row", children: [canChangePw ? (_jsx(ActionButton, { busy: busyAction === `user:${user.user_id}:pw`, icon: _jsx(SlidersHorizontal, { size: 14 }), label: "Password", onClick: () => { setPwUserId(user.user_id); setPwValue(''); } })) : null, admin && !user.built_in ? (user.disabled ? (_jsx(ActionButton, { busy: busyAction === `user:${user.user_id}:enable`, icon: _jsx(Play, { size: 14 }), label: "Enable", onClick: () => runAction(`user:${user.user_id}:enable`, () => enableUser(user.user_id)) })) : (_jsx(ActionButton, { busy: busyAction === `user:${user.user_id}:disable`, icon: _jsx(Pause, { size: 14 }), label: "Disable", onClick: () => runAction(`user:${user.user_id}:disable`, () => disableUser(user.user_id)) }))) : null, admin && !user.built_in ? (_jsx(ActionButton, { busy: busyAction === `user:${user.user_id}:delete`, danger: true, icon: _jsx(Trash2, { size: 14 }), label: "Delete", onClick: () => runAction(`user:${user.user_id}:delete`, () => deleteUser(user.user_id)) })) : null] }) })] }, user.user_id));
                            }) })] }) })] }));
}
function SettingsPage({ runAction, }) {
    const [draft, setDraft] = React.useState({ webhook_url: '', offline_after_seconds: 120, escalation_timeout_seconds: 300, metrics_retention_days: 30 });
    const [loaded, setLoaded] = React.useState(false);
    React.useEffect(() => {
        void fetchSettings().then((s) => {
            setDraft(s);
            setLoaded(true);
        }).catch(() => setLoaded(true));
    }, []);
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [_jsx("div", { className: "management-header", children: _jsx("h1", { children: "Settings" }) }), !loaded ? _jsx("div", { className: "banner", children: "Loading settings..." }) : null, loaded ? (_jsxs("form", { className: "settings-form", onSubmit: (event) => {
                    event.preventDefault();
                    void runAction('settings:save', () => updateSettings(draft));
                }, children: [_jsxs("div", { className: "form-grid", children: [_jsxs("label", { children: ["Webhook URL", _jsx("input", { placeholder: "https://hooks.example.com/\u2026", type: "url", value: draft.webhook_url, onChange: (e) => setDraft((d) => ({ ...d, webhook_url: e.target.value })) })] }), _jsxs("label", { children: ["Offline after (seconds)", _jsx("input", { min: 10, step: 1, type: "number", value: draft.offline_after_seconds, onChange: (e) => setDraft((d) => ({ ...d, offline_after_seconds: Number(e.target.value) })) })] }), _jsxs("label", { children: ["Escalation timeout (seconds)", _jsx("input", { min: 60, step: 1, type: "number", value: draft.escalation_timeout_seconds, onChange: (e) => setDraft((d) => ({ ...d, escalation_timeout_seconds: Number(e.target.value) })) })] }), _jsxs("label", { children: ["Metrics retention (days)", _jsx("input", { min: 1, max: 365, step: 1, type: "number", value: draft.metrics_retention_days, onChange: (e) => setDraft((d) => ({ ...d, metrics_retention_days: Number(e.target.value) })) })] })] }), _jsx("div", { className: "button-row", children: _jsx("button", { className: "text-button", type: "submit", children: "Save Settings" }) })] })) : null] }));
}
function AgentIdentity({ id, name, prefix }) {
    return (_jsxs("div", { children: [_jsxs("div", { className: "server-name", children: [prefix, _jsx("span", { children: name || id })] }), _jsx("div", { className: "agent-id", children: id })] }));
}
function StatusDot({ color, label }) {
    return _jsxs("span", { className: "state-note", children: [_jsx("span", { className: `dot ${color}` }), " ", label] });
}
function ActionButton({ busy, danger = false, disabled = false, icon, label, onClick, type = 'button' }) {
    return (_jsxs("button", { className: `text-button ${danger ? 'danger' : ''}`, disabled: disabled || busy, onClick: onClick, type: type, children: [icon, busy ? '...' : label] }));
}
function SummaryCard({ label, value, color }) {
    return (_jsxs("div", { className: "summary-card", children: [_jsx("div", { className: "summary-label", children: label }), _jsx("div", { className: `summary-value ${color}`, children: value })] }));
}
createRoot(document.getElementById('root')).render(_jsx(React.StrictMode, { children: _jsx(Dashboard, {}) }));
