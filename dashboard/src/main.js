import { jsx as _jsx, jsxs as _jsxs, Fragment as _Fragment } from "react/jsx-runtime";
import React from 'react';
import { createRoot } from 'react-dom/client';
import { Check, LogOut, Pause, Play, Plus, RefreshCw, RotateCw, Search, SlidersHorizontal, Trash2, X } from 'lucide-react';
import { canManageAgent, maintenanceAction, maintenanceActionLabel } from './agentActions';
import { acknowledgeAlert, approveAgent, createGroup, createUser, deleteAgent, deleteAlert, fetchSession, loadDashboardData, login, logout, rejectAgent, requestAgentStatus, restartAgentCollectors, resumeAgent, setAgentGroups, setAgentInterval, setAgentProcessLimit, setAgentThresholds, setMaintenance, } from './api';
import { groupOverviewAgents, summaryCounts, toDashboardAgents, toDisplayAlerts } from './status';
import './styles.css';
const COMPONENT_LABELS = ['CPU', 'Memory', 'Disk', 'Network', 'Temp', 'Proc', 'Heartbeat'];
const THRESHOLD_INDICATORS = ['cpu', 'memory', 'disk', 'network'];
const THRESHOLD_LEVELS = [
    'warning_pct_of_avg',
    'degraded_pct_of_avg',
    'critical_pct_of_avg',
];
const THRESHOLD_LABELS = {
    warning_pct_of_avg: 'Warn',
    degraded_pct_of_avg: 'Degrade',
    critical_pct_of_avg: 'Critical',
};
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
    const [users, setUsers] = React.useState([]);
    const [expanded, setExpanded] = React.useState(new Set());
    const [loadedAt, setLoadedAt] = React.useState(null);
    const [error, setError] = React.useState(null);
    const [loading, setLoading] = React.useState(true);
    const [busyAction, setBusyAction] = React.useState(null);
    const [intervalDrafts, setIntervalDrafts] = React.useState({});
    const [processDrafts, setProcessDrafts] = React.useState({});
    const [groupDrafts, setGroupDrafts] = React.useState({});
    const [thresholdDrafts, setThresholdDrafts] = React.useState({});
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
            setUsers(data.users);
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
    return (_jsxs(_Fragment, { children: [_jsxs("header", { className: "topbar", children: [_jsxs("div", { className: "logo", children: [_jsx("span", { className: "logo-dot" }), "THEWATCHER"] }), _jsxs("nav", { className: "nav-tabs", "aria-label": "Dashboard views", children: [_jsx(Tab, { view: view, target: "monitoring", setView: setView, label: "Monitoring" }), _jsx(Tab, { view: view, target: "agents", setView: setView, label: "Agents" }), admin ? _jsx(Tab, { view: view, target: "pending", setView: setView, label: "Pending" }) : null, _jsx(Tab, { view: view, target: "alerts", setView: setView, label: "Alerts" }), admin ? _jsx(Tab, { view: view, target: "users", setView: setView, label: "Users" }) : null] }), _jsxs("div", { className: "topbar-meta", children: [_jsx("span", { children: _jsx("strong", { children: loadedAt ? loadedAt.toUTCString().replace('GMT', 'UTC') : 'Not synced' }) }), _jsxs("span", { children: [session.username, " ", _jsx("strong", { children: session.role })] }), _jsx("button", { className: "icon-button", onClick: () => void refresh(), "aria-label": "Refresh dashboard", children: _jsx(RefreshCw, { size: 14 }) }), _jsx("button", { className: "icon-button", onClick: () => void logout().then(() => setSession(null)), "aria-label": "Log out", children: _jsx(LogOut, { size: 14 }) })] })] }), _jsxs("section", { className: "summary", children: [_jsx(SummaryCard, { label: "Healthy", value: counts.green, color: "green" }), _jsx(SummaryCard, { label: "Warning", value: counts.yellow, color: "yellow" }), _jsx(SummaryCard, { label: "Degraded", value: counts.amber, color: "amber" }), _jsx(SummaryCard, { label: "Critical", value: counts.red, color: "red" }), _jsx(SummaryCard, { label: "Maintenance", value: counts.blue, color: "blue" }), _jsx(SummaryCard, { label: "Offline", value: counts.offline, color: "grey" })] }), alerts.length > 0 && view === 'monitoring' ? (_jsx("div", { className: "alert-strip", children: displayAlerts.slice(0, 4).map((alert) => (_jsxs("div", { className: `alert-card ${alert.new_status}`, children: [_jsx(AgentIdentity, { id: alert.agentId, name: alert.agentName }), _jsxs("div", { className: "alert-card-detail", children: [_jsx("span", { children: alert.indicator }), _jsx("strong", { children: colorLabel(alert.new_status) })] }), _jsx("div", { className: "alert-card-message", children: alert.message })] }, alert.alert_id))) })) : null, view === 'monitoring' ? (_jsx(MonitoringTable, { agents: agents, error: error, expanded: expanded, groupFilter: overviewGroupFilter, groups: groups, loading: loading, setExpanded: setExpanded, setGroupFilter: setOverviewGroupFilter })) : null, view === 'agents' ? (_jsx(AgentManagement, { agents: agents, busyAction: busyAction, error: error, groupDrafts: groupDrafts, groups: groups, intervalDrafts: intervalDrafts, loading: loading, admin: admin, operator: operator, processDrafts: processDrafts, runAction: runAction, setGroupDrafts: setGroupDrafts, setIntervalDrafts: setIntervalDrafts, setProcessDrafts: setProcessDrafts, setThresholdDrafts: setThresholdDrafts, thresholdDrafts: thresholdDrafts })) : null, view === 'pending' ? (_jsx(PendingEnrollments, { agents: pending, busyAction: busyAction, groups: groups, runAction: runAction })) : null, view === 'alerts' ? (_jsx(AlertsPage, { alerts: displayAlerts, busyAction: busyAction, operator: operator, runAction: runAction })) : null, view === 'users' ? _jsx(UsersPage, { busyAction: busyAction, groups: groups, runAction: runAction, users: users }) : null] }));
}
function LoginScreen({ onLogin }) {
    const [username, setUsername] = React.useState('thewatcher');
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
function MonitoringTable({ agents, error, expanded, groupFilter, groups, loading, setExpanded, setGroupFilter, }) {
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
                                                }, children: [_jsx("td", { children: _jsx(AgentIdentity, { id: agent.id, name: agent.name, prefix: _jsx("span", { className: "chevron", children: ">" }) }) }), _jsx("td", { className: "uptime", children: agent.uptime }), _jsx("td", { className: "dot-cell", children: _jsx(StatusDot, { color: agent.status, label: colorLabel(agent.status) }) }), _jsx("td", { className: "dot-cell", children: _jsx(StatusDot, { color: agent.alertColor, label: "Alerts" }) }), agent.components.map((component) => (_jsx("td", { className: "dot-cell", children: _jsxs("div", { className: "dot-wrap", children: [_jsx("span", { className: `dot ${component.color}` }), _jsxs("div", { className: "tooltip", children: [component.label, ": ", component.value, _jsx("br", {}), _jsx("span", { children: component.detail })] })] }) }, component.key)))] }), _jsx("tr", { className: `detail-row ${expanded.has(agent.id) ? '' : 'hidden'}`, children: _jsx("td", { colSpan: 4 + COMPONENT_LABELS.length, children: _jsx("div", { className: "detail-inner", children: agent.components.map((component) => (_jsxs("div", { className: "detail-card", children: [_jsxs("div", { className: "detail-card-header", children: [_jsx("span", { children: component.label }), _jsx("span", { className: `dot ${component.color} small-dot` })] }), _jsx("div", { className: `detail-card-value ${component.color}`, children: component.value }), _jsxs("div", { className: "detail-card-sub", children: [colorLabel(component.color), " / ", component.detail] })] }, component.key))) }) }) })] }, `${group.id}:${agent.id}`)))] }, group.id))) })] }) })] }));
}
function AgentManagement({ admin, agents, busyAction, error, groupDrafts, groups, intervalDrafts, loading, operator, processDrafts, runAction, setGroupDrafts, setIntervalDrafts, setProcessDrafts, setThresholdDrafts, thresholdDrafts, }) {
    const numericDraft = (value, fallback) => {
        const parsed = Number.parseInt(value ?? '', 10);
        return Number.isFinite(parsed) && parsed > 0 ? parsed : fallback;
    };
    const thresholdKey = (indicator, level) => `${indicator}.${level}`;
    const thresholdDraftValue = (agent, indicator, level) => thresholdDrafts[agent.id]?.[thresholdKey(indicator, level)] ?? String(agent.thresholds[indicator][level]);
    const setThresholdDraft = (agent, indicator, level, value) => {
        setThresholdDrafts((current) => ({
            ...current,
            [agent.id]: {
                ...(current[agent.id] ?? {}),
                [thresholdKey(indicator, level)]: value,
            },
        }));
    };
    const thresholdPayload = (agent) => {
        const next = structuredClone(agent.thresholds);
        THRESHOLD_INDICATORS.forEach((indicator) => {
            THRESHOLD_LEVELS.forEach((level) => {
                const parsed = Number.parseFloat(thresholdDraftValue(agent, indicator, level));
                next[indicator][level] = Number.isFinite(parsed) && parsed > 0 ? parsed : agent.thresholds[indicator][level];
            });
        });
        return next;
    };
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [error ? _jsxs("div", { className: "banner error", children: ["API error: ", error] }) : null, loading ? _jsx("div", { className: "banner", children: "Loading dashboard data..." }) : null, _jsx("div", { className: "management-header", children: _jsx("h1", { children: "Agent Management" }) }), _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Agent" }), _jsx("th", { children: "Platform" }), _jsx("th", { children: "State" }), _jsx("th", { children: "Last Seen" }), _jsx("th", { children: "Groups" }), _jsx("th", { children: "Settings" }), _jsx("th", { children: "Commands" }), _jsx("th", { children: "Delete" })] }) }), _jsx("tbody", { children: agents.map((agent) => {
                                const intervalValue = intervalDrafts[agent.id] ?? String(agent.collectionInterval);
                                const processValue = processDrafts[agent.id] ?? String(agent.processLimit);
                                const groupValue = groupDrafts[agent.id] ?? String(agent.groupIds[0] ?? groups[0]?.group_id ?? 0);
                                const manageable = operator && canManageAgent(agent);
                                const maintenanceCommand = maintenanceAction(agent);
                                return (_jsxs("tr", { children: [_jsx("td", { children: _jsx(AgentIdentity, { id: agent.id, name: agent.name }) }), _jsx("td", { children: agent.platform }), _jsx("td", { children: _jsx(StatusDot, { color: agent.status, label: colorLabel(agent.status) }) }), _jsx("td", { className: "uptime", children: agent.lastSeen > 0 ? `${Math.floor((Date.now() - agent.lastSeen) / 60000)}m` : 'never' }), _jsx("td", { children: _jsxs("div", { className: "settings-grid group-settings", children: [_jsxs("select", { disabled: !admin, value: groupValue, onChange: (event) => setGroupDrafts((current) => ({ ...current, [agent.id]: event.target.value })), children: [_jsx("option", { value: 0, children: "No group" }), groups.map((group) => (_jsx("option", { value: group.group_id, children: group.name }, group.group_id)))] }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:groups`, disabled: !admin, icon: _jsx(SlidersHorizontal, { size: 14 }), label: "Set", onClick: () => runAction(`${agent.id}:groups`, () => setAgentGroups(agent.id, Number(groupValue) > 0 ? [Number(groupValue)] : [])) })] }) }), _jsx("td", { children: _jsxs("div", { className: "settings-grid", children: [_jsxs("label", { children: ["Interval", _jsx("input", { min: "1", type: "number", value: intervalValue, onChange: (event) => setIntervalDrafts((current) => ({ ...current, [agent.id]: event.target.value })) })] }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:interval`, disabled: !manageable, icon: _jsx(SlidersHorizontal, { size: 14 }), label: "Set", onClick: () => runAction(`${agent.id}:interval`, () => setAgentInterval(agent.id, numericDraft(intervalValue, 30))) }), _jsxs("label", { children: ["Processes", _jsx("input", { min: "1", type: "number", value: processValue, onChange: (event) => setProcessDrafts((current) => ({ ...current, [agent.id]: event.target.value })) })] }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:processes`, disabled: !manageable, icon: _jsx(SlidersHorizontal, { size: 14 }), label: "Set", onClick: () => runAction(`${agent.id}:processes`, () => setAgentProcessLimit(agent.id, numericDraft(processValue, 10))) }), _jsx("div", { className: "threshold-settings", children: THRESHOLD_INDICATORS.map((indicator) => (_jsxs("fieldset", { children: [_jsx("legend", { children: indicator }), THRESHOLD_LEVELS.map((level) => (_jsxs("label", { children: [THRESHOLD_LABELS[level], _jsx("input", { min: "1", step: "1", type: "number", value: thresholdDraftValue(agent, indicator, level), onChange: (event) => setThresholdDraft(agent, indicator, level, event.target.value) })] }, level)))] }, indicator))) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:thresholds`, disabled: !manageable, icon: _jsx(SlidersHorizontal, { size: 14 }), label: "Thresholds", onClick: () => runAction(`${agent.id}:thresholds`, () => setAgentThresholds(agent.id, thresholdPayload(agent))) })] }) }), _jsx("td", { children: _jsxs("div", { className: "button-row", children: [_jsx(ActionButton, { busy: busyAction === `${agent.id}:${maintenanceCommand}`, disabled: !manageable, icon: agent.maintenance ? _jsx(Play, { size: 14 }) : _jsx(Pause, { size: 14 }), label: maintenanceActionLabel(agent), onClick: () => runAction(`${agent.id}:${maintenanceCommand}`, () => agent.maintenance ? resumeAgent(agent.id) : setMaintenance(agent.id, 'operator request', 0)) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:restart`, disabled: !manageable, icon: _jsx(RotateCw, { size: 14 }), label: "Restart", onClick: () => runAction(`${agent.id}:restart`, () => restartAgentCollectors(agent.id)) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:status`, disabled: !manageable, icon: _jsx(Search, { size: 14 }), label: "Status", onClick: () => runAction(`${agent.id}:status`, () => requestAgentStatus(agent.id)) })] }) }), _jsx("td", { children: _jsx(ActionButton, { busy: busyAction === `${agent.id}:delete`, danger: true, disabled: !manageable, icon: _jsx(Trash2, { size: 14 }), label: "Delete", onClick: () => runAction(`${agent.id}:delete`, () => deleteAgent(agent.id)) }) })] }, agent.id));
                            }) })] }) })] }));
}
function PendingEnrollments({ agents, busyAction, groups, runAction }) {
    const defaultGroup = groups[0]?.group_id;
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [_jsx("div", { className: "management-header", children: _jsx("h1", { children: "Pending Enrollments" }) }), _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Agent" }), _jsx("th", { children: "Platform" }), _jsx("th", { children: "Last Seen" }), _jsx("th", { children: "Actions" })] }) }), _jsx("tbody", { children: agents.map((agent) => (_jsxs("tr", { children: [_jsx("td", { children: _jsx(AgentIdentity, { id: agent.id, name: agent.name }) }), _jsx("td", { children: agent.platform }), _jsx("td", { className: "uptime", children: agent.lastSeen > 0 ? `${Math.floor((Date.now() - agent.lastSeen) / 60000)}m` : 'never' }), _jsx("td", { children: _jsxs("div", { className: "button-row", children: [_jsx(ActionButton, { busy: busyAction === `${agent.id}:approve`, icon: _jsx(Check, { size: 14 }), label: "Approve", onClick: () => runAction(`${agent.id}:approve`, () => approveAgent(agent.id, defaultGroup ? [defaultGroup] : [])) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:reject`, icon: _jsx(X, { size: 14 }), label: "Reject", onClick: () => runAction(`${agent.id}:reject`, () => rejectAgent(agent.id)) })] }) })] }, agent.id))) })] }) })] }));
}
function AlertsPage({ alerts, busyAction, operator, runAction }) {
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [_jsx("div", { className: "management-header", children: _jsx("h1", { children: "Alerts" }) }), _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Agent" }), _jsx("th", { children: "Indicator" }), _jsx("th", { children: "State" }), _jsx("th", { children: "Created" }), _jsx("th", { children: "Message" }), _jsx("th", { children: "Actions" })] }) }), _jsx("tbody", { children: alerts.map((alert) => (_jsxs("tr", { children: [_jsx("td", { children: _jsx(AgentIdentity, { id: alert.agentId, name: alert.agentName }) }), _jsx("td", { children: alert.indicator }), _jsx("td", { children: _jsx(StatusDot, { color: alert.new_status, label: colorLabel(alert.new_status) }) }), _jsx("td", { className: "uptime", children: new Date(alert.created_at).toLocaleString() }), _jsx("td", { children: alert.message }), _jsx("td", { children: _jsxs("div", { className: "button-row", children: [_jsx(ActionButton, { busy: busyAction === `alert:${alert.alert_id}:ack`, disabled: !operator, icon: _jsx(Check, { size: 14 }), label: "Ack", onClick: () => runAction(`alert:${alert.alert_id}:ack`, () => acknowledgeAlert(alert.alert_id)) }), _jsx(ActionButton, { busy: busyAction === `alert:${alert.alert_id}:delete`, danger: true, disabled: !operator, icon: _jsx(Trash2, { size: 14 }), label: "Delete", onClick: () => runAction(`alert:${alert.alert_id}:delete`, () => deleteAlert(alert.alert_id)) })] }) })] }, alert.alert_id))) })] }) })] }));
}
function UsersPage({ busyAction, groups, runAction, users, }) {
    const [groupName, setGroupName] = React.useState('');
    const [username, setUsername] = React.useState('');
    const [password, setPassword] = React.useState('');
    const [role, setRole] = React.useState('viewer');
    const [groupId, setGroupId] = React.useState(groups[0]?.group_id ?? 0);
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [_jsx("div", { className: "management-header", children: _jsx("h1", { children: "Users & Groups" }) }), _jsxs("div", { className: "management-grid", children: [_jsxs("form", { className: "inline-form", onSubmit: (event) => {
                            event.preventDefault();
                            if (!groupName.trim())
                                return;
                            void runAction('group:create', () => createGroup(groupName.trim())).then(() => setGroupName(''));
                        }, children: [_jsx("input", { "aria-label": "Group name", placeholder: "Group name", value: groupName, onChange: (event) => setGroupName(event.target.value) }), _jsx(ActionButton, { busy: busyAction === 'group:create', icon: _jsx(Plus, { size: 14 }), label: "Group", onClick: () => undefined })] }), _jsxs("form", { className: "inline-form user-form", onSubmit: (event) => {
                            event.preventDefault();
                            if (!username.trim() || !password)
                                return;
                            void runAction('user:create', () => createUser(username.trim(), password, role, groupId ? [groupId] : [])).then(() => {
                                setUsername('');
                                setPassword('');
                            });
                        }, children: [_jsx("input", { "aria-label": "Username", placeholder: "Username", value: username, onChange: (event) => setUsername(event.target.value) }), _jsx("input", { "aria-label": "Password", placeholder: "Password", type: "password", value: password, onChange: (event) => setPassword(event.target.value) }), _jsxs("select", { "aria-label": "Role", value: role, onChange: (event) => setRole(event.target.value), children: [_jsx("option", { value: "viewer", children: "Viewer" }), _jsx("option", { value: "operator", children: "Operator" }), _jsx("option", { value: "admin", children: "Admin" })] }), _jsxs("select", { "aria-label": "Group", value: groupId, onChange: (event) => setGroupId(Number(event.target.value)), children: [_jsx("option", { value: 0, children: "No group" }), groups.map((group) => (_jsx("option", { value: group.group_id, children: group.name }, group.group_id)))] }), _jsx(ActionButton, { busy: busyAction === 'user:create', icon: _jsx(Plus, { size: 14 }), label: "User", onClick: () => undefined })] })] }), _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "User" }), _jsx("th", { children: "Role" }), _jsx("th", { children: "Groups" }), _jsx("th", { children: "State" })] }) }), _jsx("tbody", { children: users.map((user) => (_jsxs("tr", { children: [_jsx("td", { children: user.username }), _jsx("td", { children: user.role }), _jsx("td", { children: user.group_ids.map((id) => groups.find((group) => group.group_id === id)?.name ?? id).join(', ') }), _jsx("td", { children: user.disabled ? 'Disabled' : 'Active' })] }, user.user_id))) })] }) })] }));
}
function AgentIdentity({ id, name, prefix }) {
    return (_jsxs("div", { children: [_jsxs("div", { className: "server-name", children: [prefix, _jsx("span", { children: name || id })] }), _jsx("div", { className: "agent-id", children: id })] }));
}
function StatusDot({ color, label }) {
    return _jsxs("span", { className: "state-note", children: [_jsx("span", { className: `dot ${color}` }), " ", label] });
}
function ActionButton({ busy, danger = false, disabled = false, icon, label, onClick }) {
    return (_jsxs("button", { className: `text-button ${danger ? 'danger' : ''}`, disabled: disabled || busy, onClick: onClick, children: [icon, busy ? '...' : label] }));
}
function SummaryCard({ label, value, color }) {
    return (_jsxs("div", { className: "summary-card", children: [_jsx("div", { className: "summary-label", children: label }), _jsx("div", { className: `summary-value ${color}`, children: value })] }));
}
createRoot(document.getElementById('root')).render(_jsx(React.StrictMode, { children: _jsx(Dashboard, {}) }));
