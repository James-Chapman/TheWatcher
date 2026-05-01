import { afterEach, describe, expect, it, vi } from 'vitest';
import { approveAgent, createGroup, createUser, deleteAgent, pauseAgent, rejectAgent, requestAgentStatus, restartAgentCollectors, resumeAgent, setAgentGroups, setAgentInterval, setAgentProcessLimit, setAgentThresholds, } from './api';
describe('GIVEN agent management API actions', () => {
    afterEach(() => {
        vi.unstubAllGlobals();
    });
    it('WHEN enrollment actions are requested THEN the expected backend endpoints are called', async () => {
        const fetchMock = vi.fn().mockResolvedValue({ ok: true, json: async () => ({ ok: true }) });
        vi.stubGlobal('fetch', fetchMock);
        await approveAgent('agent-1', [1]);
        await setAgentGroups('agent-1', [2]);
        await rejectAgent('agent-1');
        await deleteAgent('agent-1');
        expect(fetchMock).toHaveBeenNthCalledWith(1, '/api/agents/agent-1/approve', {
            body: JSON.stringify({ group_ids: [1] }),
            credentials: 'include',
            headers: { 'Content-Type': 'application/json' },
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(2, '/api/agents/agent-1/groups', {
            body: JSON.stringify({ group_ids: [2] }),
            credentials: 'include',
            headers: { 'Content-Type': 'application/json' },
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(3, '/api/agents/agent-1/reject', {
            credentials: 'include',
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(4, '/api/agents/agent-1', {
            credentials: 'include',
            method: 'DELETE',
        });
    });
    it('WHEN settings are changed THEN command payloads are sent to the agent endpoints', async () => {
        const fetchMock = vi.fn().mockResolvedValue({ ok: true, json: async () => ({ ok: true }) });
        vi.stubGlobal('fetch', fetchMock);
        await setAgentInterval('agent-2', 30);
        await setAgentProcessLimit('agent-2', 12);
        await setAgentThresholds('agent-2', {
            cpu: { warning_pct_of_avg: 125, degraded_pct_of_avg: 150, critical_pct_of_avg: 200 },
            memory: { warning_pct_of_avg: 126, degraded_pct_of_avg: 151, critical_pct_of_avg: 201 },
            disk: { warning_pct_of_avg: 127, degraded_pct_of_avg: 152, critical_pct_of_avg: 202 },
            network: { warning_pct_of_avg: 128, degraded_pct_of_avg: 153, critical_pct_of_avg: 203 },
        });
        await pauseAgent('agent-2');
        await resumeAgent('agent-2');
        await restartAgentCollectors('agent-2');
        await requestAgentStatus('agent-2');
        expect(fetchMock).toHaveBeenNthCalledWith(1, '/api/agents/agent-2/set_interval', {
            body: JSON.stringify({ interval_seconds: 30 }),
            credentials: 'include',
            headers: { 'Content-Type': 'application/json' },
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(2, '/api/agents/agent-2/set_process_limit', {
            body: JSON.stringify({ limit: 12 }),
            credentials: 'include',
            headers: { 'Content-Type': 'application/json' },
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(3, '/api/agents/agent-2/thresholds', {
            body: JSON.stringify({
                thresholds: {
                    cpu: { warning_pct_of_avg: 125, degraded_pct_of_avg: 150, critical_pct_of_avg: 200 },
                    memory: { warning_pct_of_avg: 126, degraded_pct_of_avg: 151, critical_pct_of_avg: 201 },
                    disk: { warning_pct_of_avg: 127, degraded_pct_of_avg: 152, critical_pct_of_avg: 202 },
                    network: { warning_pct_of_avg: 128, degraded_pct_of_avg: 153, critical_pct_of_avg: 203 },
                },
            }),
            credentials: 'include',
            headers: { 'Content-Type': 'application/json' },
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(4, '/api/agents/agent-2/pause', { credentials: 'include', method: 'POST' });
        expect(fetchMock).toHaveBeenNthCalledWith(5, '/api/agents/agent-2/resume', { credentials: 'include', method: 'POST' });
        expect(fetchMock).toHaveBeenNthCalledWith(6, '/api/agents/agent-2/restart_collectors', {
            credentials: 'include',
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(7, '/api/agents/agent-2/get_status', {
            credentials: 'include',
            method: 'POST',
        });
    });
    it('WHEN users and groups are created THEN admin payloads are sent to the backend', async () => {
        const fetchMock = vi.fn().mockResolvedValue({ ok: true, json: async () => ({ user_id: 7, group_id: 3 }) });
        vi.stubGlobal('fetch', fetchMock);
        await createGroup('Production');
        await createUser('operator1', 'secret', 'operator', [3]);
        expect(fetchMock).toHaveBeenNthCalledWith(1, '/api/groups', {
            body: JSON.stringify({ name: 'Production' }),
            credentials: 'include',
            headers: { 'Content-Type': 'application/json' },
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(2, '/api/users', {
            body: JSON.stringify({ username: 'operator1', password: 'secret', role: 'operator', group_ids: [3] }),
            credentials: 'include',
            headers: { 'Content-Type': 'application/json' },
            method: 'POST',
        });
    });
});
