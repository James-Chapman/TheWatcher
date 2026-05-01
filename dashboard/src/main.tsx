import React from 'react';
import { createRoot } from 'react-dom/client';
import { Check, LogOut, Pause, Play, Plus, RefreshCw, RotateCw, Search, SlidersHorizontal, Trash2, X } from 'lucide-react';
import { canManageAgent, maintenanceAction, maintenanceActionLabel } from './agentActions';
import {
  acknowledgeAlert,
  approveAgent,
  createGroup,
  createUser,
  deleteAgent,
  deleteAlert,
  fetchSession,
  loadDashboardData,
  login,
  logout,
  rejectAgent,
  requestAgentStatus,
  restartAgentCollectors,
  resumeAgent,
  setAgentGroups,
  setAgentInterval,
  setAgentProcessLimit,
  setAgentThresholds,
  setMaintenance,
} from './api';
import type {
  AgentThresholds,
  AlertRecord,
  DashboardAgent,
  GroupRecord,
  HealthColor,
  IndicatorThresholds,
  SessionInfo,
  UserRecord,
} from './models';
import { groupOverviewAgents, summaryCounts, toDashboardAgents, toDisplayAlerts, type OverviewGroupFilter } from './status';
import './styles.css';

const COMPONENT_LABELS = ['CPU', 'Memory', 'Disk', 'Network', 'Temp', 'Proc', 'Heartbeat'];
const THRESHOLD_INDICATORS: Array<keyof AgentThresholds> = ['cpu', 'memory', 'disk', 'network'];
const THRESHOLD_LEVELS: Array<keyof IndicatorThresholds> = [
  'warning_pct_of_avg',
  'degraded_pct_of_avg',
  'critical_pct_of_avg',
];
const THRESHOLD_LABELS: Record<keyof IndicatorThresholds, string> = {
  warning_pct_of_avg: 'Warn',
  degraded_pct_of_avg: 'Degrade',
  critical_pct_of_avg: 'Critical',
};

type View = 'monitoring' | 'agents' | 'pending' | 'alerts' | 'users';

function colorLabel(color: HealthColor): string {
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
  const [session, setSession] = React.useState<SessionInfo | null>(null);
  const [view, setView] = React.useState<View>('monitoring');
  const [agents, setAgents] = React.useState<DashboardAgent[]>([]);
  const [pending, setPending] = React.useState<DashboardAgent[]>([]);
  const [groups, setGroups] = React.useState<GroupRecord[]>([]);
  const [alerts, setAlerts] = React.useState<AlertRecord[]>([]);
  const [users, setUsers] = React.useState<UserRecord[]>([]);
  const [expanded, setExpanded] = React.useState<Set<string>>(new Set());
  const [loadedAt, setLoadedAt] = React.useState<Date | null>(null);
  const [error, setError] = React.useState<string | null>(null);
  const [loading, setLoading] = React.useState(true);
  const [busyAction, setBusyAction] = React.useState<string | null>(null);
  const [intervalDrafts, setIntervalDrafts] = React.useState<Record<string, string>>({});
  const [processDrafts, setProcessDrafts] = React.useState<Record<string, string>>({});
  const [groupDrafts, setGroupDrafts] = React.useState<Record<string, string>>({});
  const [thresholdDrafts, setThresholdDrafts] = React.useState<Record<string, Record<string, string>>>({});
  const [overviewGroupFilter, setOverviewGroupFilter] = React.useState<OverviewGroupFilter>('all');

  const refresh = React.useCallback(async () => {
    if (!session) return;
    try {
      setError(null);
      const data = await loadDashboardData();
      setAgents(toDashboardAgents(data.agents, data.metrics, data.alerts));
      setPending(toDashboardAgents(data.pending, [], []));
      setGroups(data.groups);
      setAlerts(data.alerts);
      setUsers(data.users);
      setLoadedAt(new Date());
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load dashboard data');
    } finally {
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

  const runAction = React.useCallback(
    async (key: string, action: () => Promise<void>) => {
      try {
        setBusyAction(key);
        setError(null);
        await action();
        await refresh();
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Action failed');
      } finally {
        setBusyAction(null);
      }
    },
    [refresh],
  );

  if (!session) {
    return <LoginScreen onLogin={setSession} />;
  }

  const counts = summaryCounts(agents);
  const displayAlerts = toDisplayAlerts(alerts, agents);
  const admin = session.role === 'admin';
  const operator = session.role === 'admin' || session.role === 'operator';

  return (
    <>
      <header className="topbar">
        <div className="logo">
          <span className="logo-dot" />
          THEWATCHER
        </div>
        <nav className="nav-tabs" aria-label="Dashboard views">
          <Tab view={view} target="monitoring" setView={setView} label="Monitoring" />
          <Tab view={view} target="agents" setView={setView} label="Agents" />
          {admin ? <Tab view={view} target="pending" setView={setView} label="Pending" /> : null}
          <Tab view={view} target="alerts" setView={setView} label="Alerts" />
          {admin ? <Tab view={view} target="users" setView={setView} label="Users" /> : null}
        </nav>
        <div className="topbar-meta">
          <span>
            <strong>{loadedAt ? loadedAt.toUTCString().replace('GMT', 'UTC') : 'Not synced'}</strong>
          </span>
          <span>
            {session.username} <strong>{session.role}</strong>
          </span>
          <button className="icon-button" onClick={() => void refresh()} aria-label="Refresh dashboard">
            <RefreshCw size={14} />
          </button>
          <button className="icon-button" onClick={() => void logout().then(() => setSession(null))} aria-label="Log out">
            <LogOut size={14} />
          </button>
        </div>
      </header>

      <section className="summary">
        <SummaryCard label="Healthy" value={counts.green} color="green" />
        <SummaryCard label="Warning" value={counts.yellow} color="yellow" />
        <SummaryCard label="Degraded" value={counts.amber} color="amber" />
        <SummaryCard label="Critical" value={counts.red} color="red" />
        <SummaryCard label="Maintenance" value={counts.blue} color="blue" />
        <SummaryCard label="Offline" value={counts.offline} color="grey" />
      </section>

      {alerts.length > 0 && view === 'monitoring' ? (
        <div className="alert-strip">
          {displayAlerts.slice(0, 4).map((alert) => (
            <div className={`alert-card ${alert.new_status}`} key={alert.alert_id}>
              <AgentIdentity id={alert.agentId} name={alert.agentName} />
              <div className="alert-card-detail">
                <span>{alert.indicator}</span>
                <strong>{colorLabel(alert.new_status)}</strong>
              </div>
              <div className="alert-card-message">{alert.message}</div>
            </div>
          ))}
        </div>
      ) : null}

      {view === 'monitoring' ? (
        <MonitoringTable
          agents={agents}
          error={error}
          expanded={expanded}
          groupFilter={overviewGroupFilter}
          groups={groups}
          loading={loading}
          setExpanded={setExpanded}
          setGroupFilter={setOverviewGroupFilter}
        />
      ) : null}
      {view === 'agents' ? (
        <AgentManagement
          agents={agents}
          busyAction={busyAction}
          error={error}
          groupDrafts={groupDrafts}
          groups={groups}
          intervalDrafts={intervalDrafts}
          loading={loading}
          admin={admin}
          operator={operator}
          processDrafts={processDrafts}
          runAction={runAction}
          setGroupDrafts={setGroupDrafts}
          setIntervalDrafts={setIntervalDrafts}
          setProcessDrafts={setProcessDrafts}
          setThresholdDrafts={setThresholdDrafts}
          thresholdDrafts={thresholdDrafts}
        />
      ) : null}
      {view === 'pending' ? (
        <PendingEnrollments agents={pending} busyAction={busyAction} groups={groups} runAction={runAction} />
      ) : null}
      {view === 'alerts' ? (
        <AlertsPage alerts={displayAlerts} busyAction={busyAction} operator={operator} runAction={runAction} />
      ) : null}
      {view === 'users' ? <UsersPage busyAction={busyAction} groups={groups} runAction={runAction} users={users} /> : null}
    </>
  );
}

function LoginScreen({ onLogin }: { onLogin: (session: SessionInfo) => void }) {
  const [username, setUsername] = React.useState('thewatcher');
  const [password, setPassword] = React.useState('');
  const [error, setError] = React.useState<string | null>(null);
  return (
    <main className="login-wrap">
      <form
        className="login-panel"
        onSubmit={(event) => {
          event.preventDefault();
          void login(username, password).then(onLogin).catch((err) => setError(err instanceof Error ? err.message : 'Login failed'));
        }}
      >
        <div className="logo login-logo">
          <span className="logo-dot" />
          THEWATCHER
        </div>
        {error ? <div className="banner error">Login failed: {error}</div> : null}
        <label>
          Username
          <input value={username} onChange={(event) => setUsername(event.target.value)} />
        </label>
        <label>
          Password
          <input type="password" value={password} onChange={(event) => setPassword(event.target.value)} />
        </label>
        <button className="text-button" type="submit">Sign in</button>
      </form>
    </main>
  );
}

function Tab({ view, target, setView, label }: { view: View; target: View; setView: (view: View) => void; label: string }) {
  return (
    <button className={view === target ? 'active' : ''} onClick={() => setView(target)}>
      {label}
    </button>
  );
}

function MonitoringTable({
  agents,
  error,
  expanded,
  groupFilter,
  groups,
  loading,
  setExpanded,
  setGroupFilter,
}: {
  agents: DashboardAgent[];
  error: string | null;
  expanded: Set<string>;
  groupFilter: OverviewGroupFilter;
  groups: GroupRecord[];
  loading: boolean;
  setExpanded: React.Dispatch<React.SetStateAction<Set<string>>>;
  setGroupFilter: React.Dispatch<React.SetStateAction<OverviewGroupFilter>>;
}) {
  const overviewGroups = groupOverviewAgents(agents, groups, groupFilter);
  const visibleAgentCount = overviewGroups.reduce((count, group) => count + group.agents.length, 0);

  return (
    <main className="table-wrap">
      {error ? <div className="banner error">API error: {error}</div> : null}
      {loading ? <div className="banner">Loading dashboard data...</div> : null}
      {!loading && agents.length === 0 ? <div className="banner">No approved agents are reporting yet.</div> : null}
      {!loading && agents.length > 0 && visibleAgentCount === 0 ? <div className="banner">No agents match this group filter.</div> : null}

      <div className="overview-toolbar">
        <label>
          <SlidersHorizontal size={14} />
          <span>Group</span>
          <select value={groupFilter} onChange={(event) => setGroupFilter(event.target.value)}>
            <option value="all">All groups</option>
            {groups.map((group) => (
              <option key={group.group_id} value={String(group.group_id)}>{group.name}</option>
            ))}
            <option value="ungrouped">Ungrouped</option>
          </select>
        </label>
      </div>

      <div className="table-container">
        <table>
          <thead>
            <tr>
              <th>Server</th>
              <th>Uptime</th>
              <th>Status</th>
              <th>Alerts</th>
              {COMPONENT_LABELS.map((label) => (
                <th key={label}>{label}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {overviewGroups.map((group) => (
              <React.Fragment key={group.id}>
                <tr className="section-header">
                  <td colSpan={4 + COMPONENT_LABELS.length}>
                    {group.name} <span>{group.agents.length}</span>
                  </td>
                </tr>
                {group.agents.map((agent) => (
                  <React.Fragment key={`${group.id}:${agent.id}`}>
                    <tr
                      className={expanded.has(agent.id) ? 'expanded' : ''}
                      onClick={() => {
                        setExpanded((current) => {
                          const next = new Set(current);
                          if (next.has(agent.id)) next.delete(agent.id);
                          else next.add(agent.id);
                          return next;
                        });
                      }}
                    >
                      <td><AgentIdentity id={agent.id} name={agent.name} prefix={<span className="chevron">&gt;</span>} /></td>
                      <td className="uptime">{agent.uptime}</td>
                      <td className="dot-cell"><StatusDot color={agent.status} label={colorLabel(agent.status)} /></td>
                      <td className="dot-cell"><StatusDot color={agent.alertColor} label="Alerts" /></td>
                      {agent.components.map((component) => (
                        <td className="dot-cell" key={component.key}>
                          <div className="dot-wrap">
                            <span className={`dot ${component.color}`} />
                            <div className="tooltip">
                              {component.label}: {component.value}
                              <br />
                              <span>{component.detail}</span>
                            </div>
                          </div>
                        </td>
                      ))}
                    </tr>
                    <tr className={`detail-row ${expanded.has(agent.id) ? '' : 'hidden'}`}>
                      <td colSpan={4 + COMPONENT_LABELS.length}>
                        <div className="detail-inner">
                          {agent.components.map((component) => (
                            <div className="detail-card" key={component.key}>
                              <div className="detail-card-header">
                                <span>{component.label}</span>
                                <span className={`dot ${component.color} small-dot`} />
                              </div>
                              <div className={`detail-card-value ${component.color}`}>{component.value}</div>
                              <div className="detail-card-sub">
                                {colorLabel(component.color)} / {component.detail}
                              </div>
                            </div>
                          ))}
                        </div>
                      </td>
                    </tr>
                  </React.Fragment>
                ))}
              </React.Fragment>
            ))}
          </tbody>
        </table>
      </div>
    </main>
  );
}

function AgentManagement({
  admin,
  agents,
  busyAction,
  error,
  groupDrafts,
  groups,
  intervalDrafts,
  loading,
  operator,
  processDrafts,
  runAction,
  setGroupDrafts,
  setIntervalDrafts,
  setProcessDrafts,
  setThresholdDrafts,
  thresholdDrafts,
}: {
  admin: boolean;
  agents: DashboardAgent[];
  busyAction: string | null;
  error: string | null;
  groupDrafts: Record<string, string>;
  groups: GroupRecord[];
  intervalDrafts: Record<string, string>;
  loading: boolean;
  operator: boolean;
  processDrafts: Record<string, string>;
  runAction: (key: string, action: () => Promise<void>) => Promise<void>;
  setGroupDrafts: React.Dispatch<React.SetStateAction<Record<string, string>>>;
  setIntervalDrafts: React.Dispatch<React.SetStateAction<Record<string, string>>>;
  setProcessDrafts: React.Dispatch<React.SetStateAction<Record<string, string>>>;
  setThresholdDrafts: React.Dispatch<React.SetStateAction<Record<string, Record<string, string>>>>;
  thresholdDrafts: Record<string, Record<string, string>>;
}) {
  const numericDraft = (value: string | undefined, fallback: number) => {
    const parsed = Number.parseInt(value ?? '', 10);
    return Number.isFinite(parsed) && parsed > 0 ? parsed : fallback;
  };
  const thresholdKey = (indicator: keyof AgentThresholds, level: keyof IndicatorThresholds) => `${indicator}.${level}`;
  const thresholdDraftValue = (agent: DashboardAgent, indicator: keyof AgentThresholds, level: keyof IndicatorThresholds) =>
    thresholdDrafts[agent.id]?.[thresholdKey(indicator, level)] ?? String(agent.thresholds[indicator][level]);
  const setThresholdDraft = (
    agent: DashboardAgent,
    indicator: keyof AgentThresholds,
    level: keyof IndicatorThresholds,
    value: string,
  ) => {
    setThresholdDrafts((current) => ({
      ...current,
      [agent.id]: {
        ...(current[agent.id] ?? {}),
        [thresholdKey(indicator, level)]: value,
      },
    }));
  };
  const thresholdPayload = (agent: DashboardAgent): AgentThresholds => {
    const next = structuredClone(agent.thresholds);
    THRESHOLD_INDICATORS.forEach((indicator) => {
      THRESHOLD_LEVELS.forEach((level) => {
        const parsed = Number.parseFloat(thresholdDraftValue(agent, indicator, level));
        next[indicator][level] = Number.isFinite(parsed) && parsed > 0 ? parsed : agent.thresholds[indicator][level];
      });
    });
    return next;
  };

  return (
    <main className="table-wrap management-wrap">
      {error ? <div className="banner error">API error: {error}</div> : null}
      {loading ? <div className="banner">Loading dashboard data...</div> : null}
      <div className="management-header"><h1>Agent Management</h1></div>
      <div className="table-container">
        <table className="management-table">
          <thead>
            <tr>
              <th>Agent</th>
              <th>Platform</th>
              <th>State</th>
              <th>Last Seen</th>
              <th>Groups</th>
              <th>Settings</th>
              <th>Commands</th>
              <th>Delete</th>
            </tr>
          </thead>
          <tbody>
            {agents.map((agent) => {
              const intervalValue = intervalDrafts[agent.id] ?? String(agent.collectionInterval);
              const processValue = processDrafts[agent.id] ?? String(agent.processLimit);
              const groupValue = groupDrafts[agent.id] ?? String(agent.groupIds[0] ?? groups[0]?.group_id ?? 0);
              const manageable = operator && canManageAgent(agent);
              const maintenanceCommand = maintenanceAction(agent);
              return (
                <tr key={agent.id}>
                  <td><AgentIdentity id={agent.id} name={agent.name} /></td>
                  <td>{agent.platform}</td>
                  <td><StatusDot color={agent.status} label={colorLabel(agent.status)} /></td>
                  <td className="uptime">{agent.lastSeen > 0 ? `${Math.floor((Date.now() - agent.lastSeen) / 60000)}m` : 'never'}</td>
                  <td>
                    <div className="settings-grid group-settings">
                      <select disabled={!admin} value={groupValue} onChange={(event) => setGroupDrafts((current) => ({ ...current, [agent.id]: event.target.value }))}>
                        <option value={0}>No group</option>
                        {groups.map((group) => (
                          <option key={group.group_id} value={group.group_id}>{group.name}</option>
                        ))}
                      </select>
                      <ActionButton busy={busyAction === `${agent.id}:groups`} disabled={!admin} icon={<SlidersHorizontal size={14} />} label="Set" onClick={() => runAction(`${agent.id}:groups`, () => setAgentGroups(agent.id, Number(groupValue) > 0 ? [Number(groupValue)] : []))} />
                    </div>
                  </td>
                  <td>
                    <div className="settings-grid">
                      <label>Interval<input min="1" type="number" value={intervalValue} onChange={(event) => setIntervalDrafts((current) => ({ ...current, [agent.id]: event.target.value }))} /></label>
                      <ActionButton busy={busyAction === `${agent.id}:interval`} disabled={!manageable} icon={<SlidersHorizontal size={14} />} label="Set" onClick={() => runAction(`${agent.id}:interval`, () => setAgentInterval(agent.id, numericDraft(intervalValue, 30)))} />
                      <label>Processes<input min="1" type="number" value={processValue} onChange={(event) => setProcessDrafts((current) => ({ ...current, [agent.id]: event.target.value }))} /></label>
                      <ActionButton busy={busyAction === `${agent.id}:processes`} disabled={!manageable} icon={<SlidersHorizontal size={14} />} label="Set" onClick={() => runAction(`${agent.id}:processes`, () => setAgentProcessLimit(agent.id, numericDraft(processValue, 10)))} />
                      <div className="threshold-settings">
                        {THRESHOLD_INDICATORS.map((indicator) => (
                          <fieldset key={indicator}>
                            <legend>{indicator}</legend>
                            {THRESHOLD_LEVELS.map((level) => (
                              <label key={level}>
                                {THRESHOLD_LABELS[level]}
                                <input
                                  min="1"
                                  step="1"
                                  type="number"
                                  value={thresholdDraftValue(agent, indicator, level)}
                                  onChange={(event) => setThresholdDraft(agent, indicator, level, event.target.value)}
                                />
                              </label>
                            ))}
                          </fieldset>
                        ))}
                      </div>
                      <ActionButton busy={busyAction === `${agent.id}:thresholds`} disabled={!manageable} icon={<SlidersHorizontal size={14} />} label="Thresholds" onClick={() => runAction(`${agent.id}:thresholds`, () => setAgentThresholds(agent.id, thresholdPayload(agent)))} />
                    </div>
                  </td>
                  <td>
                    <div className="button-row">
                      <ActionButton busy={busyAction === `${agent.id}:${maintenanceCommand}`} disabled={!manageable} icon={agent.maintenance ? <Play size={14} /> : <Pause size={14} />} label={maintenanceActionLabel(agent)} onClick={() => runAction(`${agent.id}:${maintenanceCommand}`, () => agent.maintenance ? resumeAgent(agent.id) : setMaintenance(agent.id, 'operator request', 0))} />
                      <ActionButton busy={busyAction === `${agent.id}:restart`} disabled={!manageable} icon={<RotateCw size={14} />} label="Restart" onClick={() => runAction(`${agent.id}:restart`, () => restartAgentCollectors(agent.id))} />
                      <ActionButton busy={busyAction === `${agent.id}:status`} disabled={!manageable} icon={<Search size={14} />} label="Status" onClick={() => runAction(`${agent.id}:status`, () => requestAgentStatus(agent.id))} />
                    </div>
                  </td>
                  <td><ActionButton busy={busyAction === `${agent.id}:delete`} danger disabled={!manageable} icon={<Trash2 size={14} />} label="Delete" onClick={() => runAction(`${agent.id}:delete`, () => deleteAgent(agent.id))} /></td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </main>
  );
}

function PendingEnrollments({ agents, busyAction, groups, runAction }: { agents: DashboardAgent[]; busyAction: string | null; groups: GroupRecord[]; runAction: (key: string, action: () => Promise<void>) => Promise<void> }) {
  const defaultGroup = groups[0]?.group_id;
  return (
    <main className="table-wrap management-wrap">
      <div className="management-header"><h1>Pending Enrollments</h1></div>
      <div className="table-container">
        <table className="management-table">
          <thead><tr><th>Agent</th><th>Platform</th><th>Last Seen</th><th>Actions</th></tr></thead>
          <tbody>{agents.map((agent) => (
            <tr key={agent.id}>
              <td><AgentIdentity id={agent.id} name={agent.name} /></td>
              <td>{agent.platform}</td>
              <td className="uptime">{agent.lastSeen > 0 ? `${Math.floor((Date.now() - agent.lastSeen) / 60000)}m` : 'never'}</td>
              <td><div className="button-row">
                <ActionButton busy={busyAction === `${agent.id}:approve`} icon={<Check size={14} />} label="Approve" onClick={() => runAction(`${agent.id}:approve`, () => approveAgent(agent.id, defaultGroup ? [defaultGroup] : []))} />
                <ActionButton busy={busyAction === `${agent.id}:reject`} icon={<X size={14} />} label="Reject" onClick={() => runAction(`${agent.id}:reject`, () => rejectAgent(agent.id))} />
              </div></td>
            </tr>
          ))}</tbody>
        </table>
      </div>
    </main>
  );
}

function AlertsPage({ alerts, busyAction, operator, runAction }: { alerts: ReturnType<typeof toDisplayAlerts>; busyAction: string | null; operator: boolean; runAction: (key: string, action: () => Promise<void>) => Promise<void> }) {
  return (
    <main className="table-wrap management-wrap">
      <div className="management-header"><h1>Alerts</h1></div>
      <div className="table-container">
        <table className="management-table">
          <thead><tr><th>Agent</th><th>Indicator</th><th>State</th><th>Created</th><th>Message</th><th>Actions</th></tr></thead>
          <tbody>{alerts.map((alert) => (
            <tr key={alert.alert_id}>
              <td><AgentIdentity id={alert.agentId} name={alert.agentName} /></td>
              <td>{alert.indicator}</td>
              <td><StatusDot color={alert.new_status} label={colorLabel(alert.new_status)} /></td>
              <td className="uptime">{new Date(alert.created_at).toLocaleString()}</td>
              <td>{alert.message}</td>
              <td><div className="button-row">
                <ActionButton busy={busyAction === `alert:${alert.alert_id}:ack`} disabled={!operator} icon={<Check size={14} />} label="Ack" onClick={() => runAction(`alert:${alert.alert_id}:ack`, () => acknowledgeAlert(alert.alert_id))} />
                <ActionButton busy={busyAction === `alert:${alert.alert_id}:delete`} danger disabled={!operator} icon={<Trash2 size={14} />} label="Delete" onClick={() => runAction(`alert:${alert.alert_id}:delete`, () => deleteAlert(alert.alert_id))} />
              </div></td>
            </tr>
          ))}</tbody>
        </table>
      </div>
    </main>
  );
}

function UsersPage({
  busyAction,
  groups,
  runAction,
  users,
}: {
  busyAction: string | null;
  groups: GroupRecord[];
  runAction: (key: string, action: () => Promise<void>) => Promise<void>;
  users: UserRecord[];
}) {
  const [groupName, setGroupName] = React.useState('');
  const [username, setUsername] = React.useState('');
  const [password, setPassword] = React.useState('');
  const [role, setRole] = React.useState<UserRecord['role']>('viewer');
  const [groupId, setGroupId] = React.useState<number>(groups[0]?.group_id ?? 0);
  return (
    <main className="table-wrap management-wrap">
      <div className="management-header"><h1>Users & Groups</h1></div>
      <div className="management-grid">
        <form
          className="inline-form"
          onSubmit={(event) => {
            event.preventDefault();
            if (!groupName.trim()) return;
            void runAction('group:create', () => createGroup(groupName.trim())).then(() => setGroupName(''));
          }}
        >
          <input aria-label="Group name" placeholder="Group name" value={groupName} onChange={(event) => setGroupName(event.target.value)} />
          <ActionButton busy={busyAction === 'group:create'} icon={<Plus size={14} />} label="Group" onClick={() => undefined} />
        </form>
        <form
          className="inline-form user-form"
          onSubmit={(event) => {
            event.preventDefault();
            if (!username.trim() || !password) return;
            void runAction('user:create', () => createUser(username.trim(), password, role, groupId ? [groupId] : [])).then(() => {
              setUsername('');
              setPassword('');
            });
          }}
        >
          <input aria-label="Username" placeholder="Username" value={username} onChange={(event) => setUsername(event.target.value)} />
          <input aria-label="Password" placeholder="Password" type="password" value={password} onChange={(event) => setPassword(event.target.value)} />
          <select aria-label="Role" value={role} onChange={(event) => setRole(event.target.value as UserRecord['role'])}>
            <option value="viewer">Viewer</option>
            <option value="operator">Operator</option>
            <option value="admin">Admin</option>
          </select>
          <select aria-label="Group" value={groupId} onChange={(event) => setGroupId(Number(event.target.value))}>
            <option value={0}>No group</option>
            {groups.map((group) => (
              <option key={group.group_id} value={group.group_id}>{group.name}</option>
            ))}
          </select>
          <ActionButton busy={busyAction === 'user:create'} icon={<Plus size={14} />} label="User" onClick={() => undefined} />
        </form>
      </div>
      <div className="table-container">
        <table className="management-table">
          <thead><tr><th>User</th><th>Role</th><th>Groups</th><th>State</th></tr></thead>
          <tbody>{users.map((user) => (
            <tr key={user.user_id}>
              <td>{user.username}</td>
              <td>{user.role}</td>
              <td>{user.group_ids.map((id) => groups.find((group) => group.group_id === id)?.name ?? id).join(', ')}</td>
              <td>{user.disabled ? 'Disabled' : 'Active'}</td>
            </tr>
          ))}</tbody>
        </table>
      </div>
    </main>
  );
}

function AgentIdentity({ id, name, prefix }: { id: string; name: string; prefix?: React.ReactNode }) {
  return (
    <div>
      <div className="server-name">
        {prefix}
        <span>{name || id}</span>
      </div>
      <div className="agent-id">{id}</div>
    </div>
  );
}

function StatusDot({ color, label }: { color: HealthColor; label: string }) {
  return <span className="state-note"><span className={`dot ${color}`} /> {label}</span>;
}

function ActionButton({ busy, danger = false, disabled = false, icon, label, onClick }: { busy: boolean; danger?: boolean; disabled?: boolean; icon: React.ReactNode; label: string; onClick: () => void }) {
  return (
    <button className={`text-button ${danger ? 'danger' : ''}`} disabled={disabled || busy} onClick={onClick}>
      {icon}
      {busy ? '...' : label}
    </button>
  );
}

function SummaryCard({ label, value, color }: { label: string; value: number; color: HealthColor }) {
  return (
    <div className="summary-card">
      <div className="summary-label">{label}</div>
      <div className={`summary-value ${color}`}>{value}</div>
    </div>
  );
}

createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <Dashboard />
  </React.StrictMode>,
);
