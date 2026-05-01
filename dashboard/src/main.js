import { jsx as _jsx, jsxs as _jsxs, Fragment as _Fragment } from "react/jsx-runtime";
import React from 'react';
import { createRoot } from 'react-dom/client';
import { Check, Pause, Play, RefreshCw, RotateCw, Search, SlidersHorizontal, Trash2, X, } from 'lucide-react';
import { canManageAgent, isEnrollmentPending, maintenanceAction, maintenanceActionLabel, } from './agentActions';
import { approveAgent, deleteAgent, loadDashboardData, pauseAgent, rejectAgent, requestAgentStatus, restartAgentCollectors, resumeAgent, setAgentInterval, setAgentProcessLimit, } from './api';
import { agentStatus, summaryCounts, toDashboardAgents } from './status';
import './styles.css';
const COMPONENT_LABELS = ['CPU', 'Memory', 'Disk', 'Network', 'Temp', 'Proc', 'Approval', 'Last Seen'];
function colorLabel(color) {
    return {
        green: 'Healthy',
        yellow: 'Warning',
        orange: 'High',
        red: 'Critical',
        blue: 'Maintenance',
    }[color];
}
function enrollmentLabel(agent) {
    if (agent.rejected)
        return 'REJECTED';
    return agent.approved ? 'APPROVED' : 'PENDING';
}
function enrollmentClass(agent) {
    if (agent.rejected)
        return 'env-rejected';
    return agent.approved ? 'env-prod' : 'env-stag';
}
function Dashboard() {
    const [view, setView] = React.useState('overview');
    const [agents, setAgents] = React.useState([]);
    const [expanded, setExpanded] = React.useState(new Set());
    const [loadedAt, setLoadedAt] = React.useState(null);
    const [error, setError] = React.useState(null);
    const [loading, setLoading] = React.useState(true);
    const [busyAction, setBusyAction] = React.useState(null);
    const [intervalDrafts, setIntervalDrafts] = React.useState({});
    const [processDrafts, setProcessDrafts] = React.useState({});
    const refresh = React.useCallback(async () => {
        try {
            setError(null);
            const data = await loadDashboardData();
            setAgents(toDashboardAgents(data.agents, data.metrics));
            setLoadedAt(new Date());
        }
        catch (err) {
            setError(err instanceof Error ? err.message : 'Failed to load dashboard data');
        }
        finally {
            setLoading(false);
        }
    }, []);
    React.useEffect(() => {
        void refresh();
        const timer = window.setInterval(() => void refresh(), 5000);
        return () => window.clearInterval(timer);
    }, [refresh]);
    const runAgentAction = React.useCallback(async (agentId, actionName, action) => {
        const key = `${agentId}:${actionName}`;
        try {
            setBusyAction(key);
            setError(null);
            await action();
            await refresh();
        }
        catch (err) {
            setError(err instanceof Error ? err.message : 'Agent action failed');
        }
        finally {
            setBusyAction(null);
        }
    }, [refresh]);
    const counts = summaryCounts(agents);
    return (_jsxs(_Fragment, { children: [_jsxs("header", { className: "topbar", children: [_jsxs("div", { className: "logo", children: [_jsx("span", { className: "logo-dot" }), "THEWATCHER"] }), _jsxs("nav", { className: "nav-tabs", "aria-label": "Dashboard views", children: [_jsx("button", { className: view === 'overview' ? 'active' : '', onClick: () => setView('overview'), children: "Overview" }), _jsx("button", { className: view === 'agents' ? 'active' : '', onClick: () => setView('agents'), children: "Agents" })] }), _jsxs("div", { className: "topbar-meta", children: [_jsx("span", { children: _jsx("strong", { children: loadedAt ? loadedAt.toUTCString().replace('GMT', 'UTC') : 'Not synced' }) }), _jsxs("span", { children: ["Agents: ", _jsx("strong", { children: agents.length })] }), _jsx("button", { className: "icon-button", onClick: () => void refresh(), "aria-label": "Refresh dashboard", children: _jsx(RefreshCw, { size: 14 }) }), _jsxs("div", { className: "live-badge", children: [_jsx("span", { className: "live-pulse" }), " LIVE"] })] })] }), _jsxs("section", { className: "summary", children: [_jsx(SummaryCard, { label: "Operational", value: counts.green, color: "green" }), _jsx(SummaryCard, { label: "Degraded", value: counts.orange, color: "orange" }), _jsx(SummaryCard, { label: "Warning", value: counts.yellow, color: "yellow" }), _jsx(SummaryCard, { label: "Critical", value: counts.red, color: "red" }), _jsx(SummaryCard, { label: "Maintenance", value: counts.blue, color: "blue" })] }), view === 'overview' ? (_jsx(OverviewTable, { agents: agents, error: error, expanded: expanded, loading: loading, setExpanded: setExpanded })) : (_jsx(AgentManagement, { agents: agents, busyAction: busyAction, error: error, intervalDrafts: intervalDrafts, loading: loading, processDrafts: processDrafts, runAgentAction: runAgentAction, setIntervalDrafts: setIntervalDrafts, setProcessDrafts: setProcessDrafts }))] }));
}
function OverviewTable({ agents, error, expanded, loading, setExpanded, }) {
    const grouped = React.useMemo(() => {
        return agents.reduce((groups, agent) => {
            const group = groups.get(agent.group) ?? [];
            group.push(agent);
            groups.set(agent.group, group);
            return groups;
        }, new Map());
    }, [agents]);
    return (_jsxs("main", { className: "table-wrap", children: [error ? _jsxs("div", { className: "banner error", children: ["API error: ", error] }) : null, loading ? _jsx("div", { className: "banner", children: "Loading dashboard data..." }) : null, !loading && agents.length === 0 ? _jsx("div", { className: "banner", children: "No agents have enrolled yet." }) : null, _jsx("div", { className: "table-container", children: _jsxs("table", { children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Server" }), _jsx("th", { children: "Uptime" }), COMPONENT_LABELS.map((label) => (_jsx("th", { children: label }, label)))] }) }), _jsx("tbody", { children: [...grouped.entries()].map(([group, groupAgents]) => (_jsxs(React.Fragment, { children: [_jsx("tr", { className: "section-header", children: _jsx("td", { colSpan: 2 + COMPONENT_LABELS.length, children: group }) }), groupAgents.map((agent) => (_jsxs(React.Fragment, { children: [_jsxs("tr", { className: expanded.has(agent.id) ? 'expanded' : '', onClick: () => {
                                                    setExpanded((current) => {
                                                        const next = new Set(current);
                                                        if (next.has(agent.id))
                                                            next.delete(agent.id);
                                                        else
                                                            next.add(agent.id);
                                                        return next;
                                                    });
                                                }, children: [_jsx("td", { children: _jsxs("div", { className: "server-name", children: [_jsx("span", { className: "chevron", children: "\u203A" }), agent.name, _jsx("span", { className: `server-env ${enrollmentClass(agent)}`, children: enrollmentLabel(agent) })] }) }), _jsx("td", { className: "uptime", children: agent.uptime }), agent.components.map((component) => (_jsx("td", { className: "dot-cell", children: _jsxs("div", { className: "dot-wrap", children: [_jsx("span", { className: `dot ${component.color}` }), _jsxs("div", { className: "tooltip", children: [component.label, ": ", component.value, _jsx("br", {}), _jsx("span", { children: component.detail })] })] }) }, component.key)))] }), _jsx("tr", { className: `detail-row ${expanded.has(agent.id) ? '' : 'hidden'}`, children: _jsx("td", { colSpan: 2 + COMPONENT_LABELS.length, children: _jsx("div", { className: "detail-inner", children: agent.components.map((component) => (_jsxs("div", { className: "detail-card", children: [_jsxs("div", { className: "detail-card-header", children: [_jsx("span", { children: component.label }), _jsx("span", { className: `dot ${component.color} small-dot` })] }), _jsx("div", { className: `detail-card-value ${component.color}`, children: component.value }), _jsxs("div", { className: "detail-card-sub", children: [colorLabel(component.color), " / ", component.detail] }), component.percent !== undefined ? (_jsx("div", { className: "minibar-track", children: _jsx("div", { className: `minibar-fill ${component.color}`, style: { width: `${Math.min(component.percent, 100)}%` } }) })) : null] }, component.key))) }) }) })] }, agent.id)))] }, group))) })] }) })] }));
}
function AgentManagement({ agents, busyAction, error, intervalDrafts, loading, processDrafts, runAgentAction, setIntervalDrafts, setProcessDrafts, }) {
    const pendingCount = agents.filter((agent) => !agent.approved && !agent.rejected).length;
    const approvedCount = agents.filter((agent) => agent.approved).length;
    const rejectedCount = agents.filter((agent) => agent.rejected).length;
    const numericDraft = (value, fallback) => {
        const parsed = Number.parseInt(value ?? '', 10);
        return Number.isFinite(parsed) && parsed > 0 ? parsed : fallback;
    };
    return (_jsxs("main", { className: "table-wrap management-wrap", children: [error ? _jsxs("div", { className: "banner error", children: ["API error: ", error] }) : null, loading ? _jsx("div", { className: "banner", children: "Loading dashboard data..." }) : null, _jsx("div", { className: "management-header", children: _jsxs("div", { children: [_jsx("h1", { children: "Agent Management" }), _jsxs("div", { className: "management-counts", children: [_jsxs("span", { children: ["Approved ", approvedCount] }), _jsxs("span", { children: ["Pending ", pendingCount] }), _jsxs("span", { children: ["Rejected ", rejectedCount] })] })] }) }), _jsx("div", { className: "table-container", children: _jsxs("table", { className: "management-table", children: [_jsx("thead", { children: _jsxs("tr", { children: [_jsx("th", { children: "Agent" }), _jsx("th", { children: "Platform" }), _jsx("th", { children: "State" }), _jsx("th", { children: "Last Seen" }), _jsx("th", { children: "Enrollment" }), _jsx("th", { children: "Settings" }), _jsx("th", { children: "Commands" }), _jsx("th", { children: "Delete" })] }) }), _jsx("tbody", { children: agents.map((agent) => {
                                const intervalValue = intervalDrafts[agent.id] ?? String(agent.collectionInterval);
                                const processValue = processDrafts[agent.id] ?? String(agent.processLimit);
                                const manageable = canManageAgent(agent);
                                const pendingEnrollment = isEnrollmentPending(agent);
                                const maintenanceCommand = maintenanceAction(agent);
                                return (_jsxs("tr", { children: [_jsxs("td", { children: [_jsxs("div", { className: "server-name", children: [agent.name, _jsx("span", { className: `server-env ${enrollmentClass(agent)}`, children: enrollmentLabel(agent) })] }), _jsx("div", { className: "agent-id", children: agent.id })] }), _jsx("td", { children: agent.platform }), _jsxs("td", { children: [_jsx("span", { className: `dot ${agentStatus(agent)}` }), " ", colorLabel(agentStatus(agent))] }), _jsx("td", { className: "uptime", children: agent.components.find((component) => component.key === 'heartbeat')?.value ?? 'unknown' }), _jsx("td", { children: pendingEnrollment ? (_jsxs("div", { className: "button-row", children: [_jsx(ActionButton, { busy: busyAction === `${agent.id}:approve`, icon: _jsx(Check, { size: 14 }), label: "Approve", onClick: () => runAgentAction(agent.id, 'approve', () => approveAgent(agent.id)) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:reject`, icon: _jsx(X, { size: 14 }), label: "Reject", onClick: () => runAgentAction(agent.id, 'reject', () => rejectAgent(agent.id)) })] })) : (_jsx("span", { className: "state-note", children: agent.rejected ? 'Rejected' : 'Approved' })) }), _jsx("td", { children: manageable ? (_jsxs("div", { className: "settings-grid", children: [_jsxs("label", { children: ["Interval", _jsx("input", { min: "1", type: "number", value: intervalValue, onChange: (event) => setIntervalDrafts((current) => ({ ...current, [agent.id]: event.target.value })) })] }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:interval`, icon: _jsx(SlidersHorizontal, { size: 14 }), label: "Set", onClick: () => runAgentAction(agent.id, 'interval', () => setAgentInterval(agent.id, numericDraft(intervalValue, 30))) }), _jsxs("label", { children: ["Processes", _jsx("input", { min: "1", type: "number", value: processValue, onChange: (event) => setProcessDrafts((current) => ({ ...current, [agent.id]: event.target.value })) })] }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:processes`, icon: _jsx(SlidersHorizontal, { size: 14 }), label: "Set", onClick: () => runAgentAction(agent.id, 'processes', () => setAgentProcessLimit(agent.id, numericDraft(processValue, 10))) })] })) : (_jsx("span", { className: "state-note", children: agent.rejected ? 'Delete to re-enroll' : 'Awaiting approval' })) }), _jsx("td", { children: manageable ? (_jsxs("div", { className: "button-row", children: [_jsx(ActionButton, { busy: busyAction === `${agent.id}:${maintenanceCommand}`, icon: agent.maintenance ? _jsx(Play, { size: 14 }) : _jsx(Pause, { size: 14 }), label: maintenanceActionLabel(agent), onClick: () => runAgentAction(agent.id, maintenanceCommand, () => agent.maintenance ? resumeAgent(agent.id) : pauseAgent(agent.id)) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:restart`, icon: _jsx(RotateCw, { size: 14 }), label: "Restart", onClick: () => runAgentAction(agent.id, 'restart', () => restartAgentCollectors(agent.id)) }), _jsx(ActionButton, { busy: busyAction === `${agent.id}:status`, icon: _jsx(Search, { size: 14 }), label: "Status", onClick: () => runAgentAction(agent.id, 'status', () => requestAgentStatus(agent.id)) })] })) : (_jsx("span", { className: "state-note", children: agent.rejected ? 'No commands' : 'Pending' })) }), _jsx("td", { children: _jsx(ActionButton, { busy: busyAction === `${agent.id}:delete`, danger: true, icon: _jsx(Trash2, { size: 14 }), label: "Delete", onClick: () => runAgentAction(agent.id, 'delete', () => deleteAgent(agent.id)) }) })] }, agent.id));
                            }) })] }) })] }));
}
function ActionButton({ busy, danger = false, disabled = false, icon, label, onClick, }) {
    return (_jsxs("button", { className: `text-button ${danger ? 'danger' : ''}`, disabled: disabled || busy, onClick: onClick, children: [icon, busy ? '...' : label] }));
}
function SummaryCard({ label, value, color }) {
    return (_jsxs("div", { className: "summary-card", children: [_jsx("div", { className: "summary-label", children: label }), _jsx("div", { className: `summary-value ${color}`, children: value })] }));
}
createRoot(document.getElementById('root')).render(_jsx(React.StrictMode, { children: _jsx(Dashboard, {}) }));
