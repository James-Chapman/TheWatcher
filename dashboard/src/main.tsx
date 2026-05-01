import React from 'react';
import { createRoot } from 'react-dom/client';
import {
  Check,
  Pause,
  Play,
  RefreshCw,
  RotateCw,
  Search,
  SlidersHorizontal,
  Trash2,
  X,
} from 'lucide-react';
import {
  canManageAgent,
  isEnrollmentPending,
  maintenanceAction,
  maintenanceActionLabel,
} from './agentActions';
import {
  approveAgent,
  deleteAgent,
  loadDashboardData,
  pauseAgent,
  rejectAgent,
  requestAgentStatus,
  restartAgentCollectors,
  resumeAgent,
  setAgentInterval,
  setAgentProcessLimit,
} from './api';
import type { DashboardAgent, HealthColor } from './models';
import { agentStatus, summaryCounts, toDashboardAgents } from './status';
import './styles.css';

const COMPONENT_LABELS = ['CPU', 'Memory', 'Disk', 'Network', 'Temp', 'Proc', 'Approval', 'Last Seen'];

type View = 'overview' | 'agents';

function colorLabel(color: HealthColor): string {
  return {
    green: 'Healthy',
    yellow: 'Warning',
    orange: 'High',
    red: 'Critical',
    blue: 'Maintenance',
  }[color];
}

function enrollmentLabel(agent: DashboardAgent): string {
  if (agent.rejected) return 'REJECTED';
  return agent.approved ? 'APPROVED' : 'PENDING';
}

function enrollmentClass(agent: DashboardAgent): string {
  if (agent.rejected) return 'env-rejected';
  return agent.approved ? 'env-prod' : 'env-stag';
}

function Dashboard() {
  const [view, setView] = React.useState<View>('overview');
  const [agents, setAgents] = React.useState<DashboardAgent[]>([]);
  const [expanded, setExpanded] = React.useState<Set<string>>(new Set());
  const [loadedAt, setLoadedAt] = React.useState<Date | null>(null);
  const [error, setError] = React.useState<string | null>(null);
  const [loading, setLoading] = React.useState(true);
  const [busyAction, setBusyAction] = React.useState<string | null>(null);
  const [intervalDrafts, setIntervalDrafts] = React.useState<Record<string, string>>({});
  const [processDrafts, setProcessDrafts] = React.useState<Record<string, string>>({});

  const refresh = React.useCallback(async () => {
    try {
      setError(null);
      const data = await loadDashboardData();
      setAgents(toDashboardAgents(data.agents, data.metrics));
      setLoadedAt(new Date());
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load dashboard data');
    } finally {
      setLoading(false);
    }
  }, []);

  React.useEffect(() => {
    void refresh();
    const timer = window.setInterval(() => void refresh(), 5000);
    return () => window.clearInterval(timer);
  }, [refresh]);

  const runAgentAction = React.useCallback(
    async (agentId: string, actionName: string, action: () => Promise<void>) => {
      const key = `${agentId}:${actionName}`;
      try {
        setBusyAction(key);
        setError(null);
        await action();
        await refresh();
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Agent action failed');
      } finally {
        setBusyAction(null);
      }
    },
    [refresh],
  );

  const counts = summaryCounts(agents);

  return (
    <>
      <header className="topbar">
        <div className="logo">
          <span className="logo-dot" />
          THEWATCHER
        </div>
        <nav className="nav-tabs" aria-label="Dashboard views">
          <button className={view === 'overview' ? 'active' : ''} onClick={() => setView('overview')}>
            Overview
          </button>
          <button className={view === 'agents' ? 'active' : ''} onClick={() => setView('agents')}>
            Agents
          </button>
        </nav>
        <div className="topbar-meta">
          <span>
            <strong>{loadedAt ? loadedAt.toUTCString().replace('GMT', 'UTC') : 'Not synced'}</strong>
          </span>
          <span>
            Agents: <strong>{agents.length}</strong>
          </span>
          <button className="icon-button" onClick={() => void refresh()} aria-label="Refresh dashboard">
            <RefreshCw size={14} />
          </button>
          <div className="live-badge">
            <span className="live-pulse" /> LIVE
          </div>
        </div>
      </header>

      <section className="summary">
        <SummaryCard label="Operational" value={counts.green} color="green" />
        <SummaryCard label="Degraded" value={counts.orange} color="orange" />
        <SummaryCard label="Warning" value={counts.yellow} color="yellow" />
        <SummaryCard label="Critical" value={counts.red} color="red" />
        <SummaryCard label="Maintenance" value={counts.blue} color="blue" />
      </section>

      {view === 'overview' ? (
        <OverviewTable agents={agents} error={error} expanded={expanded} loading={loading} setExpanded={setExpanded} />
      ) : (
        <AgentManagement
          agents={agents}
          busyAction={busyAction}
          error={error}
          intervalDrafts={intervalDrafts}
          loading={loading}
          processDrafts={processDrafts}
          runAgentAction={runAgentAction}
          setIntervalDrafts={setIntervalDrafts}
          setProcessDrafts={setProcessDrafts}
        />
      )}
    </>
  );
}

function OverviewTable({
  agents,
  error,
  expanded,
  loading,
  setExpanded,
}: {
  agents: DashboardAgent[];
  error: string | null;
  expanded: Set<string>;
  loading: boolean;
  setExpanded: React.Dispatch<React.SetStateAction<Set<string>>>;
}) {
  const grouped = React.useMemo(() => {
    return agents.reduce<Map<string, DashboardAgent[]>>((groups, agent) => {
      const group = groups.get(agent.group) ?? [];
      group.push(agent);
      groups.set(agent.group, group);
      return groups;
    }, new Map());
  }, [agents]);

  return (
    <main className="table-wrap">
      {error ? <div className="banner error">API error: {error}</div> : null}
      {loading ? <div className="banner">Loading dashboard data...</div> : null}
      {!loading && agents.length === 0 ? <div className="banner">No agents have enrolled yet.</div> : null}

      <div className="table-container">
        <table>
          <thead>
            <tr>
              <th>Server</th>
              <th>Uptime</th>
              {COMPONENT_LABELS.map((label) => (
                <th key={label}>{label}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {[...grouped.entries()].map(([group, groupAgents]) => (
              <React.Fragment key={group}>
                <tr className="section-header">
                  <td colSpan={2 + COMPONENT_LABELS.length}>{group}</td>
                </tr>
                {groupAgents.map((agent) => (
                  <React.Fragment key={agent.id}>
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
                      <td>
                        <div className="server-name">
                          <span className="chevron">›</span>
                          {agent.name}
                          <span className={`server-env ${enrollmentClass(agent)}`}>{enrollmentLabel(agent)}</span>
                        </div>
                      </td>
                      <td className="uptime">{agent.uptime}</td>
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
                      <td colSpan={2 + COMPONENT_LABELS.length}>
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
                              {component.percent !== undefined ? (
                                <div className="minibar-track">
                                  <div
                                    className={`minibar-fill ${component.color}`}
                                    style={{ width: `${Math.min(component.percent, 100)}%` }}
                                  />
                                </div>
                              ) : null}
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
  agents,
  busyAction,
  error,
  intervalDrafts,
  loading,
  processDrafts,
  runAgentAction,
  setIntervalDrafts,
  setProcessDrafts,
}: {
  agents: DashboardAgent[];
  busyAction: string | null;
  error: string | null;
  intervalDrafts: Record<string, string>;
  loading: boolean;
  processDrafts: Record<string, string>;
  runAgentAction: (agentId: string, actionName: string, action: () => Promise<void>) => Promise<void>;
  setIntervalDrafts: React.Dispatch<React.SetStateAction<Record<string, string>>>;
  setProcessDrafts: React.Dispatch<React.SetStateAction<Record<string, string>>>;
}) {
  const pendingCount = agents.filter((agent) => !agent.approved && !agent.rejected).length;
  const approvedCount = agents.filter((agent) => agent.approved).length;
  const rejectedCount = agents.filter((agent) => agent.rejected).length;

  const numericDraft = (value: string | undefined, fallback: number) => {
    const parsed = Number.parseInt(value ?? '', 10);
    return Number.isFinite(parsed) && parsed > 0 ? parsed : fallback;
  };

  return (
    <main className="table-wrap management-wrap">
      {error ? <div className="banner error">API error: {error}</div> : null}
      {loading ? <div className="banner">Loading dashboard data...</div> : null}

      <div className="management-header">
        <div>
          <h1>Agent Management</h1>
          <div className="management-counts">
            <span>Approved {approvedCount}</span>
            <span>Pending {pendingCount}</span>
            <span>Rejected {rejectedCount}</span>
          </div>
        </div>
      </div>

      <div className="table-container">
        <table className="management-table">
          <thead>
            <tr>
              <th>Agent</th>
              <th>Platform</th>
              <th>State</th>
              <th>Last Seen</th>
              <th>Enrollment</th>
              <th>Settings</th>
              <th>Commands</th>
              <th>Delete</th>
            </tr>
          </thead>
          <tbody>
            {agents.map((agent) => {
              const intervalValue = intervalDrafts[agent.id] ?? String(agent.collectionInterval);
              const processValue = processDrafts[agent.id] ?? String(agent.processLimit);
              const manageable = canManageAgent(agent);
              const pendingEnrollment = isEnrollmentPending(agent);
              const maintenanceCommand = maintenanceAction(agent);
              return (
                <tr key={agent.id}>
                  <td>
                    <div className="server-name">
                      {agent.name}
                      <span className={`server-env ${enrollmentClass(agent)}`}>{enrollmentLabel(agent)}</span>
                    </div>
                    <div className="agent-id">{agent.id}</div>
                  </td>
                  <td>{agent.platform}</td>
                  <td>
                    <span className={`dot ${agentStatus(agent)}`} /> {colorLabel(agentStatus(agent))}
                  </td>
                  <td className="uptime">{agent.components.find((component) => component.key === 'heartbeat')?.value ?? 'unknown'}</td>
                  <td>
                    {pendingEnrollment ? (
                      <div className="button-row">
                        <ActionButton
                          busy={busyAction === `${agent.id}:approve`}
                          icon={<Check size={14} />}
                          label="Approve"
                          onClick={() => runAgentAction(agent.id, 'approve', () => approveAgent(agent.id))}
                        />
                        <ActionButton
                          busy={busyAction === `${agent.id}:reject`}
                          icon={<X size={14} />}
                          label="Reject"
                          onClick={() => runAgentAction(agent.id, 'reject', () => rejectAgent(agent.id))}
                        />
                      </div>
                    ) : (
                      <span className="state-note">{agent.rejected ? 'Rejected' : 'Approved'}</span>
                    )}
                  </td>
                  <td>
                    {manageable ? (
                      <div className="settings-grid">
                        <label>
                          Interval
                          <input
                            min="1"
                            type="number"
                            value={intervalValue}
                            onChange={(event) =>
                              setIntervalDrafts((current) => ({ ...current, [agent.id]: event.target.value }))
                            }
                          />
                        </label>
                        <ActionButton
                          busy={busyAction === `${agent.id}:interval`}
                          icon={<SlidersHorizontal size={14} />}
                          label="Set"
                          onClick={() =>
                            runAgentAction(agent.id, 'interval', () =>
                              setAgentInterval(agent.id, numericDraft(intervalValue, 30)),
                            )
                          }
                        />
                        <label>
                          Processes
                          <input
                            min="1"
                            type="number"
                            value={processValue}
                            onChange={(event) =>
                              setProcessDrafts((current) => ({ ...current, [agent.id]: event.target.value }))
                            }
                          />
                        </label>
                        <ActionButton
                          busy={busyAction === `${agent.id}:processes`}
                          icon={<SlidersHorizontal size={14} />}
                          label="Set"
                          onClick={() =>
                            runAgentAction(agent.id, 'processes', () =>
                              setAgentProcessLimit(agent.id, numericDraft(processValue, 10)),
                            )
                          }
                        />
                      </div>
                    ) : (
                      <span className="state-note">{agent.rejected ? 'Delete to re-enroll' : 'Awaiting approval'}</span>
                    )}
                  </td>
                  <td>
                    {manageable ? (
                      <div className="button-row">
                        <ActionButton
                          busy={busyAction === `${agent.id}:${maintenanceCommand}`}
                          icon={agent.maintenance ? <Play size={14} /> : <Pause size={14} />}
                          label={maintenanceActionLabel(agent)}
                          onClick={() =>
                            runAgentAction(agent.id, maintenanceCommand, () =>
                              agent.maintenance ? resumeAgent(agent.id) : pauseAgent(agent.id),
                            )
                          }
                        />
                        <ActionButton
                          busy={busyAction === `${agent.id}:restart`}
                          icon={<RotateCw size={14} />}
                          label="Restart"
                          onClick={() => runAgentAction(agent.id, 'restart', () => restartAgentCollectors(agent.id))}
                        />
                        <ActionButton
                          busy={busyAction === `${agent.id}:status`}
                          icon={<Search size={14} />}
                          label="Status"
                          onClick={() => runAgentAction(agent.id, 'status', () => requestAgentStatus(agent.id))}
                        />
                      </div>
                    ) : (
                      <span className="state-note">{agent.rejected ? 'No commands' : 'Pending'}</span>
                    )}
                  </td>
                  <td>
                    <ActionButton
                      busy={busyAction === `${agent.id}:delete`}
                      danger
                      icon={<Trash2 size={14} />}
                      label="Delete"
                      onClick={() => runAgentAction(agent.id, 'delete', () => deleteAgent(agent.id))}
                    />
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </main>
  );
}

function ActionButton({
  busy,
  danger = false,
  disabled = false,
  icon,
  label,
  onClick,
}: {
  busy: boolean;
  danger?: boolean;
  disabled?: boolean;
  icon: React.ReactNode;
  label: string;
  onClick: () => void;
}) {
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
