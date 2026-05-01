import type { AgentRecord, MetricsSnapshot } from './models';
export declare function fetchAgents(): Promise<AgentRecord[]>;
export declare function fetchLatestMetrics(): Promise<MetricsSnapshot[]>;
export declare function loadDashboardData(): Promise<{
    agents: AgentRecord[];
    metrics: MetricsSnapshot[];
}>;
export declare function approveAgent(agentId: string): Promise<void>;
export declare function rejectAgent(agentId: string): Promise<void>;
export declare function deleteAgent(agentId: string): Promise<void>;
export declare function setAgentInterval(agentId: string, intervalSeconds: number): Promise<void>;
export declare function setAgentProcessLimit(agentId: string, limit: number): Promise<void>;
export declare function pauseAgent(agentId: string): Promise<void>;
export declare function resumeAgent(agentId: string): Promise<void>;
export declare function restartAgentCollectors(agentId: string): Promise<void>;
export declare function requestAgentStatus(agentId: string): Promise<void>;
