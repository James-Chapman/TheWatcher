import type { AgentRecord, AlertRecord, AgentThresholds, DashboardAgent, GroupRecord, HealthColor, MetricsSnapshot } from './models';
export type SummaryKey = 'green' | 'yellow' | 'amber' | 'red' | 'blue' | 'offline';
export type OverviewGroupFilter = 'all' | 'ungrouped' | string;
export interface OverviewAgentGroup {
    id: string;
    name: string;
    agents: DashboardAgent[];
}
export interface DisplayAlert extends AlertRecord {
    agentName: string;
    agentId: string;
}
export declare const DEFAULT_THRESHOLDS: AgentThresholds;
export declare function classifyPercent(value: number): HealthColor;
export declare function worstColor(colors: HealthColor[]): HealthColor;
export declare function hostStatus(agent: Pick<DashboardAgent, 'maintenance' | 'components'>): HealthColor;
export declare function formatBytes(bytes: number): string;
export declare function formatDuration(seconds: number): string;
export declare function toDashboardAgents(agents: AgentRecord[], snapshots: MetricsSnapshot[], alerts?: AlertRecord[]): DashboardAgent[];
export declare function agentStatus(agent: DashboardAgent): HealthColor;
export declare function groupOverviewAgents(agents: DashboardAgent[], groups: GroupRecord[], filter: OverviewGroupFilter): OverviewAgentGroup[];
export declare function summaryCounts(agents: DashboardAgent[]): Record<SummaryKey, number>;
export declare function toDisplayAlerts(alerts: AlertRecord[], agents: DashboardAgent[]): DisplayAlert[];
